# Build the OPL ATA handoff payloads from the pinned opl submodule.
# Stage separate runtime payloads next to the emulator's Neutrino ELF.
[CmdletBinding()]
param(
    [string]$Image = 'ps2max/dev:v20260228'
)

$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
$oplRoot = Join-Path $workspace 'opl'
$outputRoot = Join-Path $workspace 'build-opl-ata'

if (-not (Test-Path -LiteralPath (Join-Path $oplRoot 'ee_core/Makefile'))) {
    throw 'OPL submodule is missing. Run git submodule update --init opl.'
}

$mount = '{0}:/src' -f $oplRoot
$builds = @(
    @{ Directory = 'ee_core'; Options = @() },
    @{ Directory = 'modules/iopcore/cdvdman'; Options = @('USE_BDM_ATA=1') },
    @{ Directory = 'modules/iopcore/imgdrv'; Options = @() },
    @{ Directory = 'modules/iopcore/cdvdfsv'; Options = @() },
    @{ Directory = 'modules/iopcore/resetspu'; Options = @() }
)
foreach ($build in $builds) {
    & docker run --rm -v $mount $Image make -C "/src/$($build.Directory)" @($build.Options) all
    if ($LASTEXITCODE -ne 0) {
        throw "OPL build failed: $($build.Directory)"
    }
}

New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$payloads = @(
    'ee_core/ee_core.elf',
    'modules/iopcore/cdvdman/bdm_ata_cdvdman.irx',
    'modules/iopcore/imgdrv/imgdrv.irx',
    'modules/iopcore/cdvdfsv/cdvdfsv.irx',
    'modules/iopcore/resetspu/resetspu.irx',
    'modules/iopcore/IOPRP.img'
)
foreach ($payload in $payloads) {
    Copy-Item -LiteralPath (Join-Path $oplRoot $payload) -Destination $outputRoot
}

$outputMount = '{0}:/out' -f $outputRoot
& docker run --rm -v $outputMount $Image sh -c 'cp "$PS2SDK/iop/irx/udnl.irx" /out/udnl.irx && cp "$PS2SDK/iop/irx/eesync-nano.irx" /out/eesync-nano.irx'
if ($LASTEXITCODE -ne 0) {
    throw 'Could not stage the PS2SDK handoff modules.'
}

$emulatorPayloadRoot = Join-Path $workspace 'nhddl/build-emulator/opl'
New-Item -ItemType Directory -Path $emulatorPayloadRoot -Force | Out-Null
$runtimeFiles = @(
    'ee_core.elf', 'bdm_ata_cdvdman.irx', 'cdvdfsv.irx',
    'eesync-nano.irx', 'IOPRP.img', 'udnl.irx',
    'imgdrv.irx', 'resetspu.irx'
)
foreach ($name in $runtimeFiles) {
    Copy-Item -LiteralPath (Join-Path $outputRoot $name) -Destination $emulatorPayloadRoot -Force
}

$runtimeFiles | ForEach-Object { Get-Item -LiteralPath (Join-Path $outputRoot $_) } |
    Sort-Object Name | Select-Object Name, Length
