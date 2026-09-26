# Original LUNA code: Danny Nunez (dnunezx) 2026
[CmdletBinding()]
param(
    [string]$PackagePath = '',
    [string]$LauncherPath = '',
    [string]$ArchivePath = ''
)

$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($PackagePath)) {
    $PackagePath = Join-Path $workspace 'dist/LUNA-FMCB-mc0'
}

$package = (Resolve-Path -LiteralPath $PackagePath).Path
$app = Join-Path $package 'APP_LUNA'
$required = @(
    'luna.elf',
    'luna.yaml',
    'neutrino.elf',
    'version.txt',
    'config/system.toml',
    'config/bsd-ata.toml',
    'modules/ata_bd.irx',
    'modules/ee_core.elf'
)

foreach ($relative in $required) {
    $path = Join-Path $app $relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing FMCB runtime file: $relative"
    }
}

$ambientPath = Join-Path $app 'ambient.wav'
if (-not (Test-Path -LiteralPath $ambientPath -PathType Leaf)) {
    throw 'Missing memory-card ambient soundtrack.'
}
if ((Get-Item -LiteralPath $ambientPath).Length -gt 2MB) {
    throw 'Memory-card ambient soundtrack exceeds the 2 MB package limit.'
}

$config = Get-Content -LiteralPath (Join-Path $app 'luna.yaml')
if (-not ($config -match '^mode:\s*ata\s*$')) {
    throw 'luna.yaml does not restrict LUNA to the ATA backend.'
}
if (-not ($config -match '^return_path:\s*mc0:/APP_LUNA/luna\.elf\s*$')) {
    throw 'luna.yaml does not return directly to mc0:/APP_LUNA/luna.elf.'
}

foreach ($forbidden in @('ART', 'favorites.txt', 'cache.bin', 'lastTitle.bin', 'lastView.txt', 'global.yaml', 'ambientSound.txt')) {
    if (Get-ChildItem -LiteralPath $app -Recurse -Force |
        Where-Object { $_.Name -ieq $forbidden }) {
        throw "Per-drive data must not be packaged on the memory card: $forbidden"
    }
}
if (Test-Path -LiteralPath (Join-Path $package 'ATA/LUNA/ambient.wav')) {
    throw 'The soundtrack must be packaged with APP_LUNA on the memory card.'
}

$checksumPath = Join-Path $package 'SHA256SUMS.txt'
if (-not (Test-Path -LiteralPath $checksumPath -PathType Leaf)) {
    throw 'Missing SHA256SUMS.txt.'
}
if ([IO.File]::ReadAllText($checksumPath).Contains("`r")) {
    throw 'Package checksum manifest must use LF line endings for Linux tools.'
}

$checksumLines = @(Get-Content -LiteralPath $checksumPath)
$packagedFiles = @(Get-ChildItem -LiteralPath $package -File -Recurse |
    Where-Object { $_.FullName -ne $checksumPath })
if ($checksumLines.Count -ne $packagedFiles.Count) {
    throw "Checksum manifest has $($checksumLines.Count) entries for $($packagedFiles.Count) files."
}

foreach ($line in $checksumLines) {
    if ($line -notmatch '^([0-9a-f]{64})  (.+)$') {
        throw "Malformed checksum line: $line"
    }
    $expected = $Matches[1]
    $relative = $Matches[2].Replace('/', [IO.Path]::DirectorySeparatorChar)
    $path = Join-Path $package $relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Checksum target is missing: $relative"
    }
    $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $expected) {
        throw "Checksum mismatch: $relative"
    }
}

$promotedLauncher = if ($LauncherPath) {
    $LauncherPath
} else {
    Join-Path $workspace 'dist/LUNA-Release-Candidate.elf'
}
if (Test-Path -LiteralPath $promotedLauncher -PathType Leaf) {
    $packagedLauncherHash = (Get-FileHash -LiteralPath (Join-Path $app 'luna.elf') -Algorithm SHA256).Hash
    $promotedLauncherHash = (Get-FileHash -LiteralPath $promotedLauncher -Algorithm SHA256).Hash
    if ($packagedLauncherHash -ne $promotedLauncherHash) {
        throw 'Packaged luna.elf is not the promoted Release Candidate.'
    }
}

$archive = if ($ArchivePath) { $ArchivePath } else { "$package.zip" }
if (-not (Test-Path -LiteralPath $archive -PathType Leaf)) {
    throw "Missing FMCB ZIP archive: $archive"
}
$distChecksumPath = Join-Path (Split-Path -Parent $archive) 'SHA256SUMS.txt'
if (Test-Path -LiteralPath $distChecksumPath -PathType Leaf) {
    if ([IO.File]::ReadAllText($distChecksumPath).Contains("`r")) {
        throw 'Archive checksum manifest must use LF line endings for Linux tools.'
    }
    $archiveName = Split-Path -Leaf $archive
    $matchingLines = @(Get-Content -LiteralPath $distChecksumPath |
        Where-Object { $_.EndsWith("  $archiveName") })
    if ($matchingLines.Count -ne 1 -or $matchingLines[0] -notmatch '^[0-9a-f]{64}  .+$') {
        throw "Archive checksum entry is missing or malformed: $archiveName"
    }
    $actualArchiveHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($matchingLines[0].Substring(0, 64) -ne $actualArchiveHash) {
        throw "Archive checksum mismatch: $archiveName"
    }
}

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::OpenRead($archive)
try {
    $entryNames = @($zip.Entries | ForEach-Object { $_.FullName })
    $expectedNames = @(Get-ChildItem -LiteralPath $package -File -Recurse | ForEach-Object {
        $relative = $_.FullName.Substring($package.Length + 1).Replace('\', '/')
        "$(Split-Path -Leaf $package)/$relative"
    })
    if (@($entryNames | Where-Object { $_.Contains('\') }).Count -gt 0) {
        throw 'ZIP entries contain Windows backslashes; Linux extraction may flatten the package.'
    }
    if ($entryNames.Count -ne $expectedNames.Count) {
        throw "ZIP has $($entryNames.Count) entries for $($expectedNames.Count) packaged files."
    }
    foreach ($name in $expectedNames) {
        if ($entryNames -cnotcontains $name) {
            throw "ZIP entry is missing: $name"
        }
    }
} finally {
    $zip.Dispose()
}

$bytes = (Get-ChildItem -LiteralPath $app -Recurse -File | Measure-Object Length -Sum).Sum
Write-Host "Verified FMCB package at $package"
Write-Host "Verified portable ZIP archive at $archive"
Write-Host "APP_LUNA size: $bytes bytes"
Write-Host 'Runtime is on mc0; Favorites and other library state remain on the selected game drive.'
