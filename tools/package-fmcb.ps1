# Original LUNA code: Danny Nunez (dnunezx) 2026
[CmdletBinding()]
param(
    [string]$Version = ''
)

$ErrorActionPreference = 'Stop'
if ($Version -and $Version -notmatch '^v[0-9]+\.[0-9]+\.[0-9]+(?:-rc\.[0-9]+|-beta(?:\.[0-9]+)?)?$') {
    throw "Invalid version: $Version (expected vMAJOR.MINOR.PATCH, -rc.N, or -beta[.N])."
}

$workspace = Split-Path -Parent $PSScriptRoot
$dist = Join-Path $workspace 'dist'
$packageName = if ($Version) { "LUNA-$Version-FMCB-mc0" } else { 'LUNA-FMCB-mc0' }
$launcherName = if ($Version) { "LUNA-$Version.elf" } else { 'LUNA-Release-Candidate.elf' }
$archiveName = "$packageName.zip"
$package = Join-Path $dist $packageName
$app = Join-Path $package 'APP_LUNA'
$archive = Join-Path $dist $archiveName

$launcher = Join-Path $dist $launcherName
$launcherConfig = Join-Path $workspace 'nhddl/examples/luna.yaml'
$ambientAsset = Join-Path $workspace 'nhddl/assets/ambient.wav'
$neutrinoRoot = Join-Path $workspace 'neutrino/ee/loader'
$packageReadme = Join-Path $workspace 'README.md'
$required = @(
    $launcher,
    $launcherConfig,
    $ambientAsset,
    (Join-Path $neutrinoRoot 'neutrino.elf'),
    (Join-Path $neutrinoRoot 'version.txt'),
    (Join-Path $neutrinoRoot 'config/system.toml'),
    (Join-Path $neutrinoRoot 'modules/ee_core.elf'),
    $packageReadme
)

foreach ($path in $required) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required FMCB package input is missing: $path"
    }
}

if (Test-Path -LiteralPath $package) {
    $resolvedPackage = (Resolve-Path -LiteralPath $package).Path
    $resolvedDist = (Resolve-Path -LiteralPath $dist).Path
    if ((Split-Path -Parent $resolvedPackage) -ne $resolvedDist -or
        (Split-Path -Leaf $resolvedPackage) -ne $packageName) {
        throw "Refusing to replace unexpected package path: $resolvedPackage"
    }
    Remove-Item -LiteralPath $resolvedPackage -Recurse -Force
}

New-Item -ItemType Directory -Path $app -Force | Out-Null
Copy-Item -LiteralPath $ambientAsset -Destination (Join-Path $app 'ambient.wav')
Copy-Item -LiteralPath $launcher -Destination (Join-Path $app 'luna.elf')
Copy-Item -LiteralPath $launcherConfig -Destination (Join-Path $app 'luna.yaml')
Copy-Item -LiteralPath (Join-Path $neutrinoRoot 'neutrino.elf') -Destination $app
Copy-Item -LiteralPath (Join-Path $neutrinoRoot 'version.txt') -Destination $app
Copy-Item -LiteralPath (Join-Path $neutrinoRoot 'config') -Destination $app -Recurse
Copy-Item -LiteralPath (Join-Path $neutrinoRoot 'modules') -Destination $app -Recurse
$packageReadmeText = [IO.File]::ReadAllText($packageReadme)
if ($Version) {
    $packageReadmeText = [regex]::Replace(
        $packageReadmeText,
        'LUNA-v[0-9]+\.[0-9]+\.[0-9]+(?:-rc\.[0-9]+|-beta(?:\.[0-9]+)?)?-FMCB-mc0\.zip',
        $archiveName
    )
}
[IO.File]::WriteAllText(
    (Join-Path $package 'README.md'),
    $packageReadmeText,
    [Text.UTF8Encoding]::new($false)
)

$licenseSource = Join-Path $workspace 'LICENSES'
if (Test-Path -LiteralPath $licenseSource) {
    Copy-Item -LiteralPath $licenseSource -Destination $package -Recurse
}

$packageRoot = (Resolve-Path -LiteralPath $package).Path
$checksumLines = Get-ChildItem -LiteralPath $package -File -Recurse |
    Where-Object { $_.Name -ne 'SHA256SUMS.txt' } |
    Sort-Object FullName |
    ForEach-Object {
        $relative = $_.FullName.Substring($packageRoot.Length + 1).Replace('\', '/')
        $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        "$hash  $relative"
    }
[IO.File]::WriteAllText(
    (Join-Path $package 'SHA256SUMS.txt'),
    (($checksumLines -join "`n") + "`n"),
    [Text.Encoding]::ASCII
)

if (Test-Path -LiteralPath $archive) {
    Remove-Item -LiteralPath $archive -Force
}

# Windows PowerShell's Compress-Archive writes backslashes into ZIP entry names.
# Explicit forward slashes keep nested files intact with Linux ZIP extractors.
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$packageParent = Split-Path -Parent $packageRoot
$archiveStream = [IO.File]::Open($archive, [IO.FileMode]::CreateNew)
try {
    $zip = [IO.Compression.ZipArchive]::new($archiveStream, [IO.Compression.ZipArchiveMode]::Create, $false)
    try {
        Get-ChildItem -LiteralPath $packageRoot -File -Recurse |
            Sort-Object FullName |
            ForEach-Object {
                $entryName = $_.FullName.Substring($packageParent.Length + 1).Replace('\', '/')
                [IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
                    $zip, $_.FullName, $entryName, [IO.Compression.CompressionLevel]::Optimal
                ) | Out-Null
            }
    } finally {
        $zip.Dispose()
    }
} finally {
    $archiveStream.Dispose()
}

$distChecksums = Join-Path $dist 'SHA256SUMS.txt'
$archiveHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
$distChecksumLines = @()
if (Test-Path -LiteralPath $distChecksums) {
    $distChecksumLines = @(Get-Content -LiteralPath $distChecksums |
        Where-Object { $_ -notmatch ('  ' + [regex]::Escape($archiveName) + '$') })
}
$distChecksumLines += "$archiveHash  $archiveName"
[IO.File]::WriteAllText(
    $distChecksums,
    ((@($distChecksumLines | Sort-Object) -join "`n") + "`n"),
    [Text.Encoding]::ASCII
)

Write-Host "Created $package"
Write-Host "Created $archive"
Write-Host "Updated $distChecksums"
