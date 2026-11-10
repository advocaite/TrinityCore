# Checks the installer's game folder search and path handling on folder names with [ ] and non-Latin letters (a folder like
# "[abc - def" stopped Install.cmd at step 2 with "The specified wildcard character pattern is not valid").
#   powershell -NoProfile -ExecutionPolicy Bypass -File tests\repack\test-client-search.ps1
# Exit code = number of failed checks. Works in a temp folder only and deletes it again.
param([string]$Repo = (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent), [string]$Base = (Join-Path ([IO.Path]::GetTempPath()) "wf-client-search-test"))
$ErrorActionPreference = 'Stop'
Remove-Item -LiteralPath $Base -Recurse -Force -ErrorAction SilentlyContinue
$mk = { param($p) [void][IO.Directory]::CreateDirectory($p) }
# folders that broke the old search, plus game folders with [ ] and non-Latin names
foreach ($n in '[abc - def', '【字幕】 - 第01话', '[字幕组 - 第01话', 'a[1]', ']odd', '日本語 - テスト', 'x`y') { & $mk (Join-Path $Base "scan/$n/deeper") }
$games = @((Join-Path $Base 'scan/Games [WoW]/World of Warcraft'), (Join-Path $Base 'scan/[字幕组 - 第01话/魔兽世界'))
foreach ($g in $games) {
    & $mk (Join-Path $g '_forever_')
    [IO.File]::WriteAllText((Join-Path $g '.build.info'), "Branch!STRING:0|Version!STRING:0|Product!STRING:0`neu|1.60.1.70707|wowf`n")
    [IO.File]::WriteAllText((Join-Path $g '_forever_/Wow.exe'), 'x')
}
$fail = 0
function Check($name, $ok) { if ($ok) { "PASS $name" } else { "FAIL $name"; $script:fail++ } }
# 1) the old code path crashes on these folders (proof of the bug)
try { foreach ($d in [IO.Directory]::GetDirectories((Join-Path $Base 'scan'))) { [void]@(Get-ChildItem $d -Directory -ErrorAction SilentlyContinue) }; Check 'old Get-ChildItem -Path crashes on [abc' $false }
catch { Check "old Get-ChildItem -Path crashes ($($_.Exception.Message))" ($_.Exception.Message -match 'wildcard') }
# 2) new search: no crash, both game folders found
$root = Join-Path $Base 'repackroot'; & $mk (Join-Path $root 'repack')
Set-Content -LiteralPath (Join-Path $root 'repack/client-builds.txt') "70707`n70685"
. (Join-Path $Repo 'repack/install.ps1')
$root = Join-Path $Base 'repackroot'
$found = @(Find-ClientFolders @((Join-Path $Base 'scan'), (Join-Path $Base 'missing[x')) 25 @())
Check "new search finished, found $($found.Count): $($found -join ' ; ')" ($found.Count -eq 2)
foreach ($g in $games) { Check "found $g" ($found -contains $g) }
# 3) pasted paths
$g = $games[1]
Check 'paste plain' ((Resolve-ClientInput $g) -eq $g)
Check 'paste with quotes and spaces' ((Resolve-ClientInput "  `"$g`"  ") -eq $g)
Check 'paste trailing slash' ((Resolve-ClientInput "$g/") -eq $g)
Check 'paste game folder _forever_' ((Resolve-ClientInput (Join-Path $g '_forever_')) -eq $g)
Check 'paste Wow.exe' ((Resolve-ClientInput (Join-Path $g '_forever_/Wow.exe')) -eq $g)
Check 'paste wrong folder' ($null -eq (Resolve-ClientInput (Join-Path $Base 'scan/[abc - def')))
Check 'paste empty' ($null -eq (Resolve-ClientInput '   '))
# 4) build checks on a [ ] + non-Latin client path
$p = @(Get-ClientProducts $g)
Check "Get-ClientProducts reads the [ ] client ($($p.Product) $($p.Build))" ($p.Count -eq 1 -and $p[0].Build -eq 70707 -and $p[0].Dir)
Check 'Test-ClientFolder accepts it' ($null -eq (Test-ClientFolder $root $g))
Save-Client $root $games[0] -Picked
$s = Get-SavedClient $root
Check "saved client read back ($($s.Dir))" ($s.Dir -eq $games[0] -and $s.Picked)
"`n$fail failed"
Remove-Item -LiteralPath $Base -Recurse -Force -ErrorAction SilentlyContinue
exit $fail
