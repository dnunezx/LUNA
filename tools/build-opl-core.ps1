# Original LUNA code: Danny Nunez (dnunezx) 2026
[CmdletBinding()]
param([string]$Image = 'ps2max/dev:v20260228')
$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
$oplRoot = Join-Path $workspace 'opl'
$outputRoot = Join-Path $workspace 'build-opl-core'
if (-not (Test-Path -LiteralPath (Join-Path $oplRoot 'ee_core/Makefile'))) {
    throw 'OPL submodule is missing. Run git submodule update --init opl.'
}
. (Join-Path $PSScriptRoot 'opl-runtime-files.ps1')
$mount = '{0}:/src' -f $oplRoot
$builds = @(
    @{ Directory = 'ee_core'; Options = @() },
    @{ Directory = 'modules/iopcore/cdvdman'; Options = @('USE_BDM_ATA=1') },
    @{ Directory = 'modules/iopcore/cdvdman'; Options = @('USE_BDM=1', 'USE_DEV9=1') },
    @{ Directory = 'modules/iopcore/cdvdman'; Options = @('USE_FHI=1') },
    @{ Directory = 'modules/iopcore/imgdrv'; Options = @() },
    @{ Directory = 'modules/iopcore/cdvdfsv'; Options = @() },
    @{ Directory = 'modules/iopcore/resetspu'; Options = @() }
)
foreach ($build in $builds) {
    & docker run --rm -v $mount $Image make -C "/src/$($build.Directory)" @($build.Options) all
    if ($LASTEXITCODE -ne 0) { throw "OPL build failed: $($build.Directory)" }
}
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$payloads = @(
    'ee_core/ee_core.elf',
    'modules/iopcore/cdvdman/bdm_ata_cdvdman.irx',
    'modules/iopcore/cdvdman/bdm_cdvdman.irx',
    'modules/iopcore/cdvdman/fhi_cdvdman.irx',
    'modules/iopcore/imgdrv/imgdrv.irx',
    'modules/iopcore/cdvdfsv/cdvdfsv.irx',
    'modules/iopcore/resetspu/resetspu.irx',
    'modules/iopcore/IOPRP.img'
)
foreach ($payload in $payloads) {
    Copy-Item -LiteralPath (Join-Path $oplRoot $payload) -Destination $outputRoot -Force
}
$outputMount = '{0}:/out' -f $outputRoot
& docker run --rm -v $outputMount $Image sh -c 'cp "$PS2SDK/iop/irx/udnl.irx" /out/udnl.irx && cp "$PS2SDK/iop/irx/eesync-nano.irx" /out/eesync-nano.irx'
if ($LASTEXITCODE -ne 0) { throw 'Could not stage PS2SDK handoff modules.' }
$shared = @('usbd_mini.irx', 'usbmass_bd_mini.irx', 'mx4sio_bd_mini.irx',
    'iLinkman.irx', 'IEEE1394_bd_mini.irx', 'mmcefhi.irx', 'smap.irx',
    'ministack.irx', 'udpfs_fhi.irx')
foreach ($name in $shared) {
    $source = Join-Path $workspace "neutrino/ee/loader/modules/$name"
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Build Neutrino first: missing shared device driver $name"
    }
    Copy-Item -LiteralPath $source -Destination $outputRoot -Force
}
$emulatorRoot = Join-Path $workspace 'nhddl/build-emulator/opl'
New-Item -ItemType Directory -Path $emulatorRoot -Force | Out-Null
foreach ($name in $oplRuntimeFiles) {
    Copy-Item -LiteralPath (Join-Path $outputRoot $name) -Destination $emulatorRoot -Force
}
$oplRuntimeFiles | ForEach-Object { Get-Item -LiteralPath (Join-Path $outputRoot $_) } |
    Sort-Object Name | Select-Object Name, Length
