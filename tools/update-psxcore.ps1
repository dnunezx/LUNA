# Original LUNA code: Danny Nunez (dnunezx) 2026
[CmdletBinding()]
param(
    [string]$Revision,
    [string]$BuildImage = 'ps2max/dev:v20260228',
    [switch]$Hardware,
    [switch]$Development
)
$ErrorActionPreference = 'Stop'
$workspacePath = Split-Path -Parent $PSScriptRoot
$corePath = Join-Path $workspacePath 'psxcore'
$safeCorePath = $corePath.Replace('\', '/')
function Invoke-CoreGit {
    & git -c "safe.directory=$safeCorePath" -C $corePath @args
    if ($LASTEXITCODE -ne 0) { throw "PSXCore Git command failed: $args" }
}
if (-not (Get-Command docker -ErrorAction SilentlyContinue)) { throw 'Docker is required.' }
$buildImageId = & docker image inspect --format '{{.Id}}' $BuildImage
if ($LASTEXITCODE -ne 0) { throw "Pull the SDK image before updating: docker pull $BuildImage" }
if (Invoke-CoreGit status --porcelain) {
    throw 'PSXCore has local changes. Preserve them before updating.'
}
$previousRevision = Invoke-CoreGit rev-parse HEAD
if ($Revision) {
    # Explicit revisions also support rebuilding or returning to a known commit.
    Invoke-CoreGit fetch origin
    $targetRevision = Invoke-CoreGit rev-parse --verify --end-of-options "${Revision}^{commit}"
} else {
    # Resolve the current remote default branch on every update.
    Invoke-CoreGit fetch origin HEAD
    $targetRevision = Invoke-CoreGit rev-parse --verify 'FETCH_HEAD^{commit}'
    Invoke-CoreGit merge-base --is-ancestor $previousRevision $targetRevision
}
Invoke-CoreGit checkout --detach $targetRevision
$buildName = if ($Hardware) { 'build-psxcore-hardware' } else { 'build-psxcore' }
$emulatorOption = if ($Hardware) { 'OFF' } else { 'ON' }
$developmentOption = if ($Development) { 'ON' } else { 'OFF' }
Write-Host "Building PSXCore $targetRevision (previously $previousRevision)."
$dockerArguments = @(
    'run', '--rm', '-v', "${workspacePath}:/workspace",
    '-e', "LUNA_BUILD_DIR=/workspace/nhddl/$buildName",
    '-e', "LUNA_EMULATOR_OPTION=$emulatorOption",
    '-e', "LUNA_PSXCORE_DEVELOPMENT=$developmentOption",
    '-e', "LUNA_BUILD_IMAGE=$BuildImage",
    '-e', "LUNA_BUILD_IMAGE_ID=$buildImageId",
    $buildImageId, 'sh', '/workspace/tools/build-luna-psxcore.sh'
)
& docker @dockerArguments
if ($LASTEXITCODE -ne 0) {
    throw "Build failed. PSXCore remains at $targetRevision for inspection; previous revision: $previousRevision."
}
Write-Host "Build complete: nhddl/$buildName/psxcore-build.txt"
Write-Host 'Gameplay and save/reload verification are still required. No commit or push was made.'
