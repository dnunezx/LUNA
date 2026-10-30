# Original LUNA code: Danny Nunez (dnunezx) 2026
function Assert-LunaProductionPackage {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$PackageRoot)
    $root = (Resolve-Path -LiteralPath $PackageRoot).Path
    $forbiddenFolders = @('tools', 'tests', 'test-data', 'fixtures', 'diagnostics',
        'captures', 'emulation', 'local', 'psxcore-tester')
    $forbiddenExtensions = @('.exe', '.py', '.ps1', '.sh', '.patch', '.map', '.log',
        '.trace', '.pdb', '.sym', '.o', '.obj', '.a', '.c', '.h')
    $diagnosticName = '(^|[._-])(debug|diag|diagnostics?|profil(?:e|er|ing)|probe|udptty|ps2link|ps2netfs|deci2|drvtif|test(?:er)?)([._-]|$)|psxcore.*tester|^validate-psxcore-bootstrap(?:\.|$)'
    foreach ($file in Get-ChildItem -LiteralPath $root -File -Recurse -Force) {
        $relative = $file.FullName.Substring($root.Length + 1)
        $parts = $relative -split '[\\/]'
        $blockedFolder = @($parts | Where-Object { $_ -in $forbiddenFolders }).Count -gt 0
        if ($blockedFolder -or $file.Extension -in $forbiddenExtensions -or
            $file.Name -match $diagnosticName) {
            throw "Development/diagnostic artifact must not be shipped: $relative"
        }
    }
}
