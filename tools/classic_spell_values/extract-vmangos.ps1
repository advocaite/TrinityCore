# Read-only. Copies the INSERT lines of four tables (spell_template, player_classlevelstats, creature_template, spell_affect) out of the VMaNGOS world dump into one small
# gzip file the generator reads (gen_spell_values.py). Writes nothing next to the dump, does not start MySQL or VMaNGOS.
#   powershell -ExecutionPolicy Bypass -File extract-vmangos.ps1 -Dump "C:\path\world_full_14_june_2021.sql"
# Output: data\vmangos_extract.sql.gz next to this script, and a line count per table.
param(
    [Parameter(Mandatory = $true)][string]$Dump,
    [string]$Out = (Join-Path $PSScriptRoot 'data\vmangos_extract.sql.gz')
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $Dump)) { throw "Dump not found: $Dump" }
New-Item -ItemType Directory -Force -Path (Split-Path $Out) | Out-Null

$reader = New-Object System.IO.StreamReader($Dump, [System.Text.Encoding]::UTF8)
$fileStream = [System.IO.File]::Create($Out)
$gzip = New-Object System.IO.Compression.GZipStream($fileStream, [System.IO.Compression.CompressionMode]::Compress)
$writer = New-Object System.IO.StreamWriter($gzip, (New-Object System.Text.UTF8Encoding($false)))
$writer.NewLine = "`n"
$counts = @{ spell_template = 0; player_classlevelstats = 0; creature_template = 0; spell_affect = 0 }
$current = $null
try {
    while (($line = $reader.ReadLine()) -ne $null) {
        if ($line.StartsWith('INSERT INTO')) {
            if ($line.StartsWith('INSERT INTO `spell_template`')) { $current = 'spell_template' }
            elseif ($line.StartsWith('INSERT INTO `player_classlevelstats`')) { $current = 'player_classlevelstats' }
            elseif ($line.StartsWith('INSERT INTO `creature_template`')) { $current = 'creature_template' }
            elseif ($line.StartsWith('INSERT INTO `spell_affect`')) { $current = 'spell_affect' }
            else { $current = $null }
            if ($current) { $writer.WriteLine($line) }
            continue
        }
        if ($current -and $line.StartsWith("`t(")) {
            $writer.WriteLine($line)
            $counts[$current]++
        }
    }
}
finally {
    $writer.Dispose(); $gzip.Dispose(); $fileStream.Dispose(); $reader.Dispose()
}
"spell_template rows: $($counts['spell_template'])"
"player_classlevelstats rows: $($counts['player_classlevelstats'])"
"creature_template rows: $($counts['creature_template'])"
"spell_affect rows: $($counts['spell_affect'])"
"Wrote $Out ($((Get-Item $Out).Length) bytes)"
