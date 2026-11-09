<#
.SYNOPSIS
    Extract WoW client data for TrinityCore (maps / vmaps / mmaps / dbc / Cameras).

.DESCRIPTION
    Runs the full extractor pipeline:
        mapextractor -> vmap4extractor -> vmap4assembler -> mmaps_generator
    in the correct order against a WoW client install.

    For the road-aware mmap workstream (P1.0b corpus dumping) pass -MapsOnly to
    stop after mapextractor.

    See src/modules/PlayerbotV2/docs/CLIENT_DATA_EXTRACTION.md for the full
    user-facing reference.

.PARAMETER ClientPath
    WoW install root containing .build.info / Data/ (e.g. "C:\Program Files (x86)\World of Warcraft\_retail_").

.PARAMETER OutputPath
    Where the extractor binaries live AND where the dbc/maps/vmaps/mmaps/Cameras
    output directories will be written.

.PARAMETER MapsOnly
    Skip vmap4extractor + vmap4assembler + mmaps_generator. Used for the road
    workflow which only needs maps/ + dbc/ + live CASC access.

.PARAMETER Threads
    Worker count for vmap4extractor and mmaps_generator. Default: 0 (all cores).

.PARAMETER Resume
    Safe to run again after an interruption. Each finished stage leaves a marker file (.extract-maps.done, .extract-vmaps.done,
    .extract-mmaps.done) in OutputPath and is skipped next time; an unfinished vmap stage is cleaned and redone; mmaps_generator itself
    skips tiles it already built, so the long stage continues where it stopped. At the end the intermediate Buildings/ and dir_bin/
    folders (about 40 GB) are removed and mmaps_generator runs with --silent (no key press at the end).

.PARAMETER Locale
    DBC locale (enUS, deDE, frFR, etc). Default: enUS.

.EXAMPLE
    .\extract_client_data.ps1 -ClientPath "C:\Program Files (x86)\World of Warcraft\_retail_" -OutputPath "I:\TrinityCore\playerbot\TrinityCore\build\bin\RelWithDebInfo"

.EXAMPLE
    .\extract_client_data.ps1 -ClientPath "C:\Program Files (x86)\World of Warcraft\_retail_" -OutputPath "I:\TrinityCore\playerbot\TrinityCore\build\bin\RelWithDebInfo" -MapsOnly
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ClientPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath,

    [switch]$MapsOnly,

    [int]$Threads = 0,

    [switch]$Resume,

    [string]$Locale = "enUS",

    # client builds to extract from, preferred first: the product of $ClientPath with the first of these builds is used
    [int[]]$Builds = @()
)

$ErrorActionPreference = "Stop"

function Write-Step($msg) {
    Write-Host ""
    Write-Host "==============================================================" -ForegroundColor Cyan
    Write-Host " $msg" -ForegroundColor Cyan
    Write-Host "==============================================================" -ForegroundColor Cyan
}

function Fail($msg) {
    Write-Host ""
    Write-Host "ERROR: $msg" -ForegroundColor Red
    exit 1
}

function Format-Size($bytes) {
    if ($bytes -ge 1GB) { return "{0:N2} GB" -f ($bytes / 1GB) }
    if ($bytes -ge 1MB) { return "{0:N1} MB" -f ($bytes / 1MB) }
    return "{0:N1} KB" -f ($bytes / 1KB)
}

function Show-DirSummary($root, $names) {
    Write-Host ""
    Write-Host "--- Output summary ---" -ForegroundColor Yellow
    $totalBytes = 0
    foreach ($name in $names) {
        $p = Join-Path $root $name
        if (Test-Path $p) {
            $files = Get-ChildItem -Recurse -File $p -ErrorAction SilentlyContinue
            $count = ($files | Measure-Object).Count
            $sum   = ($files | Measure-Object -Property Length -Sum).Sum
            if ($null -eq $sum) { $sum = 0 }
            $totalBytes += $sum
            Write-Host ("  {0,-12} files={1,-8} size={2}" -f $name, $count, (Format-Size $sum))
        } else {
            Write-Host ("  {0,-12} MISSING" -f $name) -ForegroundColor DarkGray
        }
    }
    Write-Host ("  {0,-12} {1}" -f "TOTAL", (Format-Size $totalBytes)) -ForegroundColor Green
}

# --- Validate output path + extractor binaries -----------------------------
$OutputPath = (Resolve-Path -LiteralPath $OutputPath -ErrorAction SilentlyContinue)
if (-not $OutputPath) {
    Fail "OutputPath does not exist."
}

$required = @("mapextractor.exe")
if (-not $MapsOnly) {
    $required += @("vmap4extractor.exe", "vmap4assembler.exe", "mmaps_generator.exe")
}

foreach ($bin in $required) {
    $p = Join-Path $OutputPath $bin
    if (-not (Test-Path $p)) {
        Fail "Required binary not found: $p
In the WoW Forever Bots package it is in the server folder: an antivirus program may have removed it (look in its quarantine or
protection history, restore it and allow the folder), or the zip was not unpacked completely (unpack the server folder again).
When building from source: build the worldserver project first (cmake --build build --config RelWithDebInfo)."
    }
}

# --- Validate client install -----------------------------------------------
if (-not (Test-Path $ClientPath)) {
    Fail "ClientPath does not exist: $ClientPath"
}
$buildInfo = Join-Path $ClientPath ".build.info"
$dataDir   = Join-Path $ClientPath "Data"
if (-not (Test-Path $buildInfo) -and -not (Test-Path $dataDir)) {
    Fail "ClientPath '$ClientPath' has no .build.info or Data\ - not a valid WoW install root.
Did you point at _retail_/Data/ instead of _retail_/ ?"
}

Write-Host "Client : $ClientPath"
Write-Host "Output : $OutputPath"
Write-Host "Locale : $Locale"
Write-Host "Threads: $(if ($Threads -le 0) { '(all cores)' } else { $Threads })"
Write-Host "Mode   : $(if ($MapsOnly) { 'MAPS-ONLY (road workflow)' } else { 'FULL' })$(if ($Resume) { ' (resumable)' })"

# map_gx_gy tiles of the six start positions worldserver checks before it starts (World.cpp ExistMapAndVMap): without them it shuts down
$StartTiles = @('0000_43_31', '0000_48_32', '0001_33_39', '0000_28_28', '0001_12_30', '0001_37_32')
function Test-Done($name) { $Resume -and (Test-Path (Join-Path $OutputPath ".extract-$name.done")) }
function Set-Done($name) { if ($Resume) { Set-Content (Join-Path $OutputPath ".extract-$name.done") (Get-Date -Format s) } }
$clock = [Diagnostics.Stopwatch]::StartNew()

# The CASC product the extractors open. The tools default to wowf since 70790 (wow_classic_beta, _classic_beta_\WowB.exe, before); the downloadable
# WoW Forever client 70532 (_forever_\Wow.exe) is product wowf, and vmap4extractor opened with the wrong product failed at once on a fresh
# install of kit 0.1.15. .build.info lists one row per installed product; take the one whose game folder is there.
# The beta and the Forever release can both be installed, with different builds: with -Builds the product of the first listed
# build is used, so the data always comes from the client the server accepts.
$Product = $null
try {
    $biLines = @(Get-Content (Join-Path $ClientPath '.build.info') -ErrorAction Stop)
    $biCols = @($biLines[0] -split '\|') | ForEach-Object { ($_ -split '!')[0] }
    $prodCol = [array]::IndexOf($biCols, 'Product'); $verCol = [array]::IndexOf($biCols, 'Version')
    if ($prodCol -ge 0) {
        $layouts = @(@('wowf', '_forever_'), @('wow_classic_beta', '_classic_beta_'))
        $rows = @($biLines | Select-Object -Skip 1 | Where-Object { $_.Trim() } | ForEach-Object {
            $f = $_ -split '\|'
            [pscustomobject]@{ Product = $f[$prodCol].Trim(); Version = $(if ($verCol -ge 0) { $f[$verCol].Trim() } else { '' }) }
        } | Where-Object { $_.Product })
        $present = @($rows | Where-Object { $p = $_.Product; @($layouts | Where-Object { $_[0] -eq $p -and (Test-Path (Join-Path $ClientPath $_[1])) }).Count })
        if ($Builds.Count) {
            foreach ($b in $Builds) {
                $hit = @($present | Where-Object { $_.Version -match "(^|\.)$b$" })
                if ($hit.Count) { $Product = $hit[0].Product; break }
            }
            if (-not $Product) {
                Fail ("No client of build $($Builds -join ' or ') in $ClientPath (it has: " + (($rows | ForEach-Object { "$($_.Product) $($_.Version)" }) -join ', ') + ').')
            }
        } else {
            foreach ($cand in $layouts) { if (@($present | Where-Object { $_.Product -eq $cand[0] }).Count) { $Product = $cand[0]; break } }
            if (-not $Product -and $rows.Count -eq 1) { $Product = $rows[0].Product }
        }
    }
} catch { if ($Builds.Count) { throw } }
if (-not $Product) { $Product = 'wowf' }   # no .build.info product column: the extractors' own guess
$productArgs = @('-p', $Product)
Write-Host "Product: $Product"

Push-Location $OutputPath
try {
    # --- Step 1: mapextractor ----------------------------------------------
    Write-Step "Step 1/4: mapextractor (maps + dbc + Cameras + gt)"
    if (Test-Done 'maps') { Write-Host "Already done (resuming)." }
    else {
        $eFlag = if ($MapsOnly) { "3" } else { "15" }
        & ".\mapextractor.exe" -i "$ClientPath" -o "$OutputPath" -e $eFlag -f 1 -l $Locale @productArgs
        if ($LASTEXITCODE -eq 2) {
            Fail "mapextractor could not read some map tiles of Eastern Kingdoms / Kalimdor from the game client (exit code 2).
The game is not completely downloaded or is damaged: let Battle.net finish downloading it, run its Scan and Repair, then run this again."
        }
        if ($LASTEXITCODE -ne 0) {
            Fail "mapextractor failed with exit code $LASTEXITCODE.
Check stdout above; usual culprits: CASC init failure, wrong client path, antivirus."
        }
        # mapextractor ends with exit code 0 when it could open no locale of the client ("No locales detected"): check what it wrote
        if (-not (Test-Path (Join-Path $OutputPath 'dbc')) -or -not (Get-ChildItem (Join-Path $OutputPath 'maps') -Filter '*.map' -File -ErrorAction SilentlyContinue | Select-Object -First 1)) {
            Fail "mapextractor could not read the game client in $ClientPath as product '$Product' (no dbc or maps were written). Check stdout above."
        }
        # the start-zone tiles worldserver checks before it starts: a run that stopped part way must not be marked done
        $noTile = @($StartTiles | Where-Object { -not (Test-Path -LiteralPath (Join-Path $OutputPath "maps\$_.map")) })
        if ($noTile.Count) {
            Fail ("mapextractor did not write every map: " + (($noTile | ForEach-Object { "maps\$_.map" }) -join ', ') + " missing. Check stdout above (antivirus, full disk, or the game client needs Battle.net's Scan and Repair), then run it again.")
        }
        Set-Done 'maps'
    }

    if ($MapsOnly) {
        Write-Step "Maps-only mode: skipping vmap4/mmaps generation."
        Show-DirSummary $OutputPath @("dbc", "maps", "Cameras", "gt")
        Write-Host ""
        Write-Host "DONE (maps-only)." -ForegroundColor Green
        return
    }

    # --- Step 2: vmap4extractor --------------------------------------------
    Write-Step "Step 2/4: vmap4extractor (CASC -> Buildings/)"
    if (Test-Done 'vmaps') { Write-Host "Already done (resuming)." }
    else {
    if ($Resume) { foreach ($junk in 'Buildings', 'dir_bin') { Remove-Item (Join-Path $OutputPath $junk) -Recurse -Force -ErrorAction SilentlyContinue } }   # a half-finished earlier run
    $vmapArgs = @("-d", "$ClientPath") + $productArgs
    if ($Threads -gt 0) { $vmapArgs += @("--threads", "$Threads") }
    & ".\vmap4extractor.exe" @vmapArgs
    $vmapExit = $LASTEXITCODE
    if ($vmapExit -ne 0 -and $Threads -ne 1) {
        # A crash with all cores (seen on a many-core VPS: 0xC0000005) is retried once on one thread, which also needs the least memory.
        # The first exit code stays in the log so the crash itself can be looked into.
        Write-Warning ("vmap4extractor stopped with exit code $vmapExit ({0}). Trying again on one thread (slower)..." -f ('0x{0:X8}' -f $vmapExit))
        foreach ($junk in 'Buildings', 'dir_bin') { Remove-Item (Join-Path $OutputPath $junk) -Recurse -Force -ErrorAction SilentlyContinue }
        & ".\vmap4extractor.exe" (@("-d", "$ClientPath") + $productArgs + @("--threads", "1"))
        $vmapExit = $LASTEXITCODE
    }
    if ($vmapExit -ne 0) {
        Fail "vmap4extractor failed with exit code $vmapExit ($('0x{0:X8}' -f $vmapExit)).
Its output above says why (the client could not be opened as product '$Product', or a leftover Buildings/dir_bin/ in $OutputPath).
If the game shows an update in the Battle.net app, let it finish first; with a game that is up to date, run Battle.net's Scan and Repair for it."
    }

    # --- Step 3: vmap4assembler --------------------------------------------
    Write-Step "Step 3/4: vmap4assembler (Buildings/ -> vmaps/)"
    $asmArgs = @("Buildings", "vmaps")
    if ($Threads -gt 0) { $asmArgs += @("--threads", "$Threads") }
    & ".\vmap4assembler.exe" @asmArgs
    if ($LASTEXITCODE -ne 0) {
        Fail "vmap4assembler failed with exit code $LASTEXITCODE."
    }
    $noTile = @($StartTiles | Where-Object { $m = $_.Substring(0, 4); -not (Test-Path -LiteralPath (Join-Path $OutputPath "vmaps\$m\$_.vmtile")) })
    foreach ($m in '0000', '0001') { if (-not (Test-Path -LiteralPath (Join-Path $OutputPath "vmaps\$m\$m.vmtree"))) { $noTile += $m } }
    if ($noTile.Count) {
        Fail ("vmap4assembler did not write every vmap the server needs at the start zones (" + ($noTile -join ', ') + " missing in vmaps). Check stdout above, then run it again.")
    }
    Set-Done 'vmaps'
    }

    # --- Step 4: mmaps_generator -------------------------------------------
    Write-Step "Step 4/4: mmaps_generator (dbc+maps+vmaps -> mmaps/)"
    $mmapArgs = @("--input", "$OutputPath", "--output", "$OutputPath", "--silent")
    if ($Threads -gt 0) { $mmapArgs += @("--threads", "$Threads") }
    # run it as a process so the progress can be shown: one line per minute with the number of maps finished so far
    $expect = @{}
    foreach ($f in Get-ChildItem (Join-Path $OutputPath 'maps') -Filter '*.tilelist' -ErrorAction SilentlyContinue) { if ($f.Name -match '^(\d{4})') { $expect[[int]$Matches[1]] = 1 } }
    foreach ($d in Get-ChildItem (Join-Path $OutputPath 'vmaps') -Directory -ErrorAction SilentlyContinue) { if ($d.Name -match '^\d+$') { $expect[[int]$d.Name] = 1 } }
    $gen = Start-Process (Join-Path (Get-Location).Path 'mmaps_generator.exe') -ArgumentList $mmapArgs -WorkingDirectory (Get-Location).Path -NoNewWindow -PassThru
    $null = $gen.Handle   # Windows PowerShell 5.1 leaves ExitCode empty unless the process handle is read right after the start
    while (-not $gen.WaitForExit(60000)) {
        $done = @(Get-ChildItem (Join-Path $OutputPath 'mmaps') -Filter '*.mmap' -ErrorAction SilentlyContinue).Count
        Write-Host ("Pathfinding data: {0} of about {1} maps done, still working ({2} minutes so far)..." -f $done, $expect.Count, [math]::Round($clock.Elapsed.TotalMinutes))
    }
    $genCode = $gen.ExitCode
    if ($genCode -ne 0) {
        Fail "mmaps_generator failed with exit code $genCode.
Check that dbc/, maps/, and vmaps/0000/*.vmtree exist in OutputPath."
    }
    # Safety net: a map that has game files but still no navmesh (the generator skips "junk"/dev maps by default) is built on its own,
    # because naming a map id builds it whatever the skip options say. Without one the server logs "Could not load MMAP ... result: 2".
    $ids = @()
    foreach ($f in Get-ChildItem (Join-Path $OutputPath 'maps') -Filter '*.tilelist' -ErrorAction SilentlyContinue) { if ($f.Name -match '^(\d{4})') { $ids += [int]$Matches[1] } }
    foreach ($d in Get-ChildItem (Join-Path $OutputPath 'vmaps') -Directory -ErrorAction SilentlyContinue) { if ($d.Name -match '^\d+$') { $ids += [int]$d.Name } }
    $missing = @($ids | Sort-Object -Unique | Where-Object { -not (Test-Path (Join-Path $OutputPath ('mmaps\{0:D4}.mmap' -f $_))) })
    foreach ($id in $missing) {
        Write-Host "Building the navigation mesh of map $id on its own..."
        $one = @("--input", "$OutputPath", "--output", "$OutputPath", "--silent", "$id")
        if ($Threads -gt 0) { $one += @("--threads", "$Threads") }
        & ".\mmaps_generator.exe" @one
    }
    $still = @($missing | Where-Object { -not (Test-Path (Join-Path $OutputPath ('mmaps\{0:D4}.mmap' -f $_))) })
    $have = @(Get-ChildItem (Join-Path $OutputPath 'mmaps') -Filter '*.mmap' -ErrorAction SilentlyContinue).Count
    Write-Host "Navigation meshes: $have maps built; all steps took $([math]::Round($clock.Elapsed.TotalMinutes)) minutes."
    if ($still.Count) { Write-Warning ("No navigation mesh could be built for map(s): " + ($still -join ', ') + ". Usually these maps have no ground to walk on.") }
    if ($have -lt 1) { Fail "No navigation meshes were produced (mmaps/ is empty)." }
    Set-Done 'mmaps'
    if ($Resume) { foreach ($junk in 'Buildings', 'dir_bin') { Remove-Item (Join-Path $OutputPath $junk) -Recurse -Force -ErrorAction SilentlyContinue } }

    Show-DirSummary $OutputPath @("dbc", "maps", "Cameras", "gt", "vmaps", "mmaps", "Buildings")
    Write-Host ""
    Write-Host "DONE (full extraction)." -ForegroundColor Green
    Write-Host "Tip: 'Buildings/' is intermediate and can be deleted to reclaim ~40 GB:" -ForegroundColor DarkGray
    Write-Host "    Remove-Item -Recurse -Force '$OutputPath\Buildings','$OutputPath\dir_bin'" -ForegroundColor DarkGray
}
finally {
    Pop-Location
}
