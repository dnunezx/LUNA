# Original LUNA code: Danny Nunez (dnunezx) 2026
# Compatibility entry point; builds every supported device backend.
[CmdletBinding()]
param([string]$Image = 'ps2max/dev:v20260228')
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'build-opl-core.ps1') -Image $Image
