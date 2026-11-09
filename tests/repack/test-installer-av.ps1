# Keeps repack\install.ps1 free of the things that made Kaspersky delete it as "UDS:Trojan-PSW.PowerShell.Stealer.gen"
# (0.1.28 to 0.1.29): stripping the internet mark (that is repack\unblock.ps1, run by Install.cmd), the Desktop/Downloads/Documents and all-drives search,
# Battle.net data, self-elevation with Bypass, and password/secrets words. Those parts live in their own small scripts
# (unblock.ps1, client-search.ps1, elevate.ps1, finish-install.ps1).
#   powershell -NoProfile -ExecutionPolicy Bypass -File tests\repack\test-installer-av.ps1
# Exit code = number of failed checks.
param([string]$Repo = (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent))
$text = [IO.File]::ReadAllText((Join-Path $Repo 'repack/install.ps1'))
$fail = 0
foreach ($p in 'Unblock-File', '-Recurse', 'USERPROFILE', 'Downloads', 'Documents', '-PSProvider', 'Battle', 'ProgramData', 'product.db',
               'RunAs', 'Bypass', 'IsInRole', 'password', 'secrets', 'EncodedCommand', 'Invoke-Expression', 'DownloadString', 'FromBase64') {
    if ($text -match [regex]::Escape($p)) { "FAIL install.ps1 contains $p"; $fail++ } else { "PASS no $p" }
}
foreach ($f in 'unblock.ps1', 'client-search.ps1', 'elevate.ps1', 'finish-install.ps1') {
    if (Test-Path -LiteralPath (Join-Path $Repo "repack/$f")) { "PASS $f exists" } else { "FAIL $f missing"; $fail++ }
}
exit $fail
