# Original LUNA code: Danny Nunez (dnunezx) 2026
[CmdletBinding()]
param(
    [string]$Version = '',
    [string]$LocalTest = '',
    [string]$PsxcoreBuildDirectory = ''
)

$ErrorActionPreference = 'Stop'
if ($LocalTest -and ($Version -or $LocalTest -notmatch '^[0-9]{8}-[0-9a-f]{7,12}(?:-dirty)?$')) {
    throw 'LocalTest must be YYYYMMDD-commit[-dirty], and cannot be combined with Version.'
}
if ($Version -and $Version -notmatch '^v[0-9]+\.[0-9]+\.[0-9]+(?:-rc\.[0-9]+|-beta(?:\.[0-9]+)?)?$') {
    throw "Invalid version: $Version (expected vMAJOR.MINOR.PATCH, -rc.N, or -beta[.N])."
}

$workspace = Split-Path -Parent $PSScriptRoot
$dist = Join-Path $workspace 'dist'
$packageName = if ($LocalTest) { "LUNA-local-test-$LocalTest-FMCB-mc0" } elseif ($Version) { "LUNA-$Version-FMCB-mc0" } else { 'LUNA-FMCB-mc0' }
$launcherName = if ($LocalTest) { "LUNA-local-test-$LocalTest.elf" } elseif ($Version) { "LUNA-$Version.elf" } else { 'LUNA-Release-Candidate.elf' }
$archiveName = "$packageName.zip"
$package = Join-Path $dist $packageName
$app = Join-Path $package 'APP_LUNA'
$ambientAddon = Join-Path $package 'Ambient-Music-Add-On'
$archive = Join-Path $dist $archiveName

$launcher = Join-Path $dist $launcherName
$launcherConfig = Join-Path $workspace 'nhddl/examples/luna.yaml'
$ambientAsset = Join-Path $workspace 'nhddl/assets/ambient.wav'
$ambientInstructions = Join-Path $workspace 'tools/ambient-music-addon.txt'
$neutrinoRoot = Join-Path $workspace 'neutrino/ee/loader'
$oplPayloadRoot = Join-Path $workspace 'build-opl-core'
. (Join-Path $PSScriptRoot 'opl-runtime-files.ps1')
. (Join-Path $PSScriptRoot 'assert-production-package.ps1')
$neutrinoRuntimeFiles = @(
    'IEEE1394_bd_mini.irx',
    'ata_bd.irx',
    'atad_emu.irx',
    'bdfs.irx',
    'bdm.irx',
    'bdmfs_fatfs.irx',
    'cdvdfsv.irx',
    'cdvdman_emu.irx',
    'cdvdman_esr1.irx',
    'cdvdman_esr2.irx',
    'dev9_hidden.irx',
    'dev9_ns.irx',
    'ee_core.elf',
    'eesync.irx',
    'fakemod.irx',
    'fhi_bd.irx',
    'fileXio.irx',
    'gapfill.irx',
    'hdlfs.irx',
    'iLinkman.irx',
    'imgdrv.irx',
    'iomanX.irx',
    'mc_emu.irx',
    'ministack.irx',
    'mmcefhi.irx',
    'mmceman.irx',
    'mx4sio_bd_mini.irx',
    'patch_freemem.irx',
    'patch_membo.irx',
    'patch_rc_uya.irx',
    'ps2fs.irx',
    'ps2hdd-bdm.irx',
    'smap.irx',
    'udnl-t300.irx',
    'udnl.irx',
    'udpbd.irx',
    'udpfs_bd.irx',
    'udpfs_fhi.irx',
    'udpfs_ioman.irx',
    'usbd_mini.irx',
    'usbd_null.irx',
    'usbmass_bd_mini.irx'
)
$packageReadme = Join-Path $workspace 'README.md'
$required = @(
    $launcher,
    $launcherConfig,
    $ambientAsset,
    $ambientInstructions,
    (Join-Path $neutrinoRoot 'neutrino.elf'),
    (Join-Path $neutrinoRoot 'version.txt'),
    (Join-Path $neutrinoRoot 'config/system.toml'),
    (Join-Path $neutrinoRoot 'modules/ee_core.elf'),
    $packageReadme
)
$required += @($oplRuntimeFiles | ForEach-Object { Join-Path $oplPayloadRoot $_ })
$required += @($neutrinoRuntimeFiles | ForEach-Object { Join-Path (Join-Path $neutrinoRoot 'modules') $_ })
if ($PsxcoreBuildDirectory) {
    $PsxcoreBuildDirectory = (Resolve-Path -LiteralPath $PsxcoreBuildDirectory).Path
    $required += @('luna.elf', 'psxcore-runtime-bootstrap.elf', 'psxcore-build.txt', 'CMakeCache.txt' |
        ForEach-Object { Join-Path $PsxcoreBuildDirectory $_ })
}

foreach ($path in $required) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required FMCB package input is missing: $path"
    }
}

if ($PsxcoreBuildDirectory) {
    $cache = [IO.File]::ReadAllText((Join-Path $PsxcoreBuildDirectory 'CMakeCache.txt'))
    foreach ($setting in @{
        CMAKE_BUILD_TYPE = 'Release'; ENABLE_PRINTF = 'OFF';
        LUNA_EMULATOR_BUILD = 'OFF'; LUNA_ENABLE_PSXCORE = 'ON';
        LUNA_PSXCORE_DEVELOPMENT = 'OFF'
    }.GetEnumerator()) {
        $pattern = '(?m)^' + [regex]::Escape($setting.Key) + ':[^=]+=' + $setting.Value + '\r?$'
        if ($cache -notmatch $pattern) { throw "Unsafe package build setting: $($setting.Key)" }
    }
    if ((Get-FileHash -LiteralPath $launcher).Hash -ne
        (Get-FileHash -LiteralPath (Join-Path $PsxcoreBuildDirectory 'luna.elf')).Hash) {
        throw 'The launcher does not match the supplied PSXCore build.'
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
New-Item -ItemType Directory -Path $ambientAddon -Force | Out-Null
Copy-Item -LiteralPath $ambientAsset -Destination (Join-Path $ambientAddon 'ambient.wav')
Copy-Item -LiteralPath $ambientInstructions -Destination (Join-Path $ambientAddon 'INSTALL.txt')
Copy-Item -LiteralPath $launcher -Destination (Join-Path $app 'luna.elf')
if ($PsxcoreBuildDirectory) {
    Copy-Item -LiteralPath (Join-Path $PsxcoreBuildDirectory 'psxcore-runtime-bootstrap.elf') -Destination $app
    Copy-Item -LiteralPath (Join-Path $PsxcoreBuildDirectory 'psxcore-build.txt') -Destination (Join-Path $package 'BUILD-INFO.txt')
}
Copy-Item -LiteralPath $launcherConfig -Destination (Join-Path $app 'luna.yaml')
Copy-Item -LiteralPath (Join-Path $neutrinoRoot 'neutrino.elf') -Destination $app
Copy-Item -LiteralPath (Join-Path $neutrinoRoot 'version.txt') -Destination $app
$configDestination = Join-Path $app 'config'
$moduleDestination = Join-Path $app 'modules'
New-Item -ItemType Directory -Path $configDestination, $moduleDestination -Force | Out-Null
Get-ChildItem -LiteralPath (Join-Path $neutrinoRoot 'config') -Filter '*.toml' -File |
    Where-Object { $_.Name -notmatch '(?i)(debug|diagnostic|profile|probe|pcsx2|test)' } |
    ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $configDestination }
foreach ($name in $neutrinoRuntimeFiles) {
    Copy-Item -LiteralPath (Join-Path (Join-Path $neutrinoRoot 'modules') $name) -Destination $moduleDestination
}
New-Item -ItemType Directory -Path (Join-Path $app 'opl') -Force | Out-Null
foreach ($name in $oplRuntimeFiles) {
    Copy-Item -LiteralPath (Join-Path $oplPayloadRoot $name) -Destination (Join-Path $app 'opl')
}
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

Assert-LunaProductionPackage -PackageRoot $package

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
