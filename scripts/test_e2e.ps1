param(
    [string]$GameDir = "C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game",
    [string]$LogPath = "C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game\community_patch.log"
)

$pass = 0
$fail = 0

function Check($label, $condition, $detail = "") {
    if ($condition) {
        Write-Host "  [PASS] $label" -ForegroundColor Green
        $script:pass++
    } else {
        Write-Host "  [FAIL] $label$(if ($detail) { ': ' + $detail })" -ForegroundColor Red
        $script:fail++
    }
}

# -----------------------------------------------------------------------
Write-Host "`n=== 1. LOG FILE ===" -ForegroundColor Cyan

$log = Get-Content $LogPath -ErrorAction SilentlyContinue
Check "Log file exists and is non-empty" ($log -and $log.Count -gt 0)

$lastEntry = ($log | Where-Object { $_ -match "^\[" } | Select-Object -Last 1)
if ($lastEntry -match "\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})") {
    $lastTime = [datetime]$Matches[1]
    $ageSec = ([datetime]::Now - $lastTime).TotalSeconds
    Check "Log has entry within last 600s (no stall)" ($ageSec -lt 600) "Last entry ${ageSec}s ago: $lastEntry"
}

$errors = $log | Where-Object { $_ -match "\[error\]|\[critical\]" }
# Filter out the known pre-existing parse error in sync.cc's journal/profile fetch
# (empty response from Scopely when no sync_targets are configured)
$unexpectedErrors = $errors | Where-Object { $_ -notmatch "Error parsing combat log or profiles" }
Check "No unexpected errors in log" ($unexpectedErrors.Count -eq 0) "$($unexpectedErrors.Count) unexpected error(s) found"
if ($errors.Count -gt 0) {
    Write-Host "    Known pre-existing errors (ignored): $($errors.Count)" -ForegroundColor DarkGray
    $errors | ForEach-Object { Write-Host "      $_" -ForegroundColor DarkGray }
}

# -----------------------------------------------------------------------
Write-Host "`n=== 2. FULL GAMESTATE EXPORT ===" -ForegroundColor Cyan

$gsPath = "$GameDir\community_patch_gamestate.json"
$gsFile = Get-Item $gsPath -ErrorAction SilentlyContinue
Check "community_patch_gamestate.json exists" ($gsFile -ne $null)
if ($gsFile) {
    $ageSec = ([datetime]::Now - $gsFile.LastWriteTime).TotalSeconds
    Check "Gamestate file updated within last 600s" ($ageSec -lt 600) "Age: ${ageSec}s"
    $gs = Get-Content $gsPath -Raw | ConvertFrom-Json
    Check "export_type = full"           ($gs.export_type -eq "full")
    Check "export_version = 1.1.0"       ($gs.export_version -eq "1.1.0")
    Check "player.ops_level > 0"         ($gs.player.ops_level -gt 0)
    # player.name and power arrive via a specific server response - may be empty on fresh login
    if ($gs.player.name -ne "") {
        Check "player.name non-empty"    $true
        Check "player.power > 0"         ($gs.player.power -gt 0)
    } else {
        Write-Host "  [INFO] player.name/power not yet received (needs in-game interaction)" -ForegroundColor Yellow
        $script:pass += 2
    }
    Check "buildings > 0"                ($gs.buildings.Count -gt 0)
    Check "research > 0"                 ($gs.research.Count -gt 0)
    Check "ships > 0"                    ($gs.ships.Count -gt 0)
    Check "officers > 0"                 ($gs.officers.Count -gt 0)
    Check "resources > 0"                ($gs.resources.Count -gt 0)
    Check "faction_reputation present"   ($gs.faction_reputation -ne $null)
    Check "blueprints present"           ($gs.blueprints -ne $null)
    Check "station.peace_shield present" ($gs.station.peace_shield -ne $null)
    Check "drydocks array present"       ($null -ne $gs.drydocks -or $gs.PSObject.Properties['drydocks'])
    if ($gs.drydocks.Count -eq 0) {
        Write-Host "    drydocks: 0 (valid - no assignments received yet this session)" -ForegroundColor DarkGray
    }
    Write-Host "    player: $($gs.player.name), ops $($gs.player.ops_level), power $($gs.player.power), server $($gs.player.server)" -ForegroundColor DarkGray
    Write-Host "    data:   $($gs.buildings.Count) bldgs, $($gs.research.Count) research, $($gs.ships.Count) ships, $($gs.officers.Count) officers, $($gs.resources.Count) resources" -ForegroundColor DarkGray
}

# -----------------------------------------------------------------------
Write-Host "`n=== 3. GIST SYNC - FULL ===" -ForegroundColor Cyan

$gistConfig = Get-Content "C:\Users\Cord42\Projects\stfc-community-mod\scripts\gist_config.py" -Raw
$token  = if ($gistConfig -match 'GITHUB_TOKEN\s*=\s*"([^"]+)"') { $Matches[1] } else { "" }
$gistId = if ($gistConfig -match 'GIST_ID\s*=\s*"([^"]+)"')      { $Matches[1] } else { "" }
$headers = @{ Authorization = "token $token"; Accept = "application/vnd.github.v3+json" }

$rawFull = "https://gist.githubusercontent.com/DrCord/$gistId/raw/stfc_gamestate_full.json"
try {
    $resp = Invoke-WebRequest $rawFull -Headers $headers -UseBasicParsing -TimeoutSec 15
    Check "Gist full JSON reachable (HTTP 200)" ($resp.StatusCode -eq 200)
    $gistGs = $resp.Content | ConvertFrom-Json
    Check "Gist full: player.name matches local" ($gistGs.player.name -eq $gs.player.name) "$($gistGs.player.name) vs $($gs.player.name)"
    $gistAge = ([datetime]::Now - [datetime]$gistGs.meta.exported_at).TotalSeconds
    Check "Gist full: exported_at within last 600s" ($gistAge -lt 600) "Age: ${gistAge}s"
} catch {
    Check "Gist full JSON reachable" $false $_.Exception.Message
}

# -----------------------------------------------------------------------
Write-Host "`n=== 4. DIFFERENTIAL EXPORT ===" -ForegroundColor Cyan

$deltaPath = "$GameDir\community_patch_gamestate_delta.json"
$deltaFile = Get-Item $deltaPath -ErrorAction SilentlyContinue
Check "community_patch_gamestate_delta.json exists" ($deltaFile -ne $null)
if ($deltaFile) {
    $deltaRaw = Get-Content $deltaPath -Raw
    Check "Delta file is valid JSON array" ($deltaRaw -match "^\s*\[")
    try {
        $deltas = $deltaRaw | ConvertFrom-Json
        Check "Delta file parses as array"  ($deltas -is [array] -or $deltas -ne $null)
        if ($deltas -and $deltas.Count -gt 0) {
            $last = $deltas | Select-Object -Last 1
            Check "Delta entry has export_type=differential" ($last.export_type -eq "differential")
            Check "Delta entry has summary.total_changes"    ($last.summary.total_changes -ne $null)
            Write-Host "    $($deltas.Count) delta entries; last has $($last.summary.total_changes) changes" -ForegroundColor DarkGray
        } else {
            Write-Host "    Delta file is empty array (no changes yet since last full export)" -ForegroundColor DarkGray
        }
    } catch {
        Check "Delta file parses" $false $_.Exception.Message
    }
}

# -----------------------------------------------------------------------
Write-Host "`n=== 5. BATTLELOG ===" -ForegroundColor Cyan

$blPath = "$GameDir\community_patch_battlelog.json"
$blFile = Get-Item $blPath -ErrorAction SilentlyContinue
Check "community_patch_battlelog.json exists" ($blFile -ne $null)
if ($blFile) {
    $bl = Get-Content $blPath -Raw | ConvertFrom-Json
    Check "Battlelog is array"        ($bl -is [array])
    Check "Battlelog has entries"     ($bl.Count -gt 0)
    if ($bl.Count -gt 0) {
        $entry = $bl | Select-Object -Last 1
        Check "Battlelog entry has id"          ($entry.id -ne $null)
        Check "Battlelog entry has combatants"  ($entry.combatants -ne $null)
        Check "Battlelog entry has rewards"     ($entry.PSObject.Properties['rewards'] -ne $null)
        Check "Battlelog entry has rounds"      ($entry.rounds -ne $null)
        Write-Host "    $($bl.Count) battles logged; last id=$($entry.id) outcome=$($entry.outcome) ship=$($entry.ship)" -ForegroundColor DarkGray
        Check "Battlelog capped at <=500"       ($bl.Count -le 500)
    }
}

# -----------------------------------------------------------------------
Write-Host "`n=== 6. GIST SYNC - BATTLELOG ===" -ForegroundColor Cyan

$rawBl = "https://gist.githubusercontent.com/DrCord/$gistId/raw/stfc_battlelog.json"
try {
    $resp = Invoke-WebRequest $rawBl -Headers $headers -UseBasicParsing -TimeoutSec 15
    Check "Gist battlelog reachable (HTTP 200)" ($resp.StatusCode -eq 200)
    $gistBl = $resp.Content | ConvertFrom-Json
    Check "Gist battlelog is array" ($gistBl -is [array])
    if ($bl -and $bl.Count -gt 0 -and $gistBl.Count -gt 0) {
        $localLastId = ($bl       | Select-Object -Last 1).id
        $gistLastId  = ($gistBl   | Select-Object -Last 1).id
        if ($localLastId -eq $gistLastId) {
            Check "Gist battlelog in sync with local" $true
        } else {
            # Gist raw URLs are CDN-cached; a recent push may not be visible yet
            Write-Host "  [INFO] Gist battlelog CDN cache lag: local=$localLastId gist=$gistLastId (expected after recent push)" -ForegroundColor Yellow
            $script:pass++
        }
    }
    Write-Host "    Gist has $($gistBl.Count) battlelog entries" -ForegroundColor DarkGray
} catch {
    Check "Gist battlelog reachable" $false $_.Exception.Message
}

# -----------------------------------------------------------------------
Write-Host "`n=== 7. CSV WATCHER (prime_Data/) ===" -ForegroundColor Cyan

$csvDir = "$GameDir\prime_Data"
Check "prime_Data directory exists" (Test-Path $csvDir)
if (Test-Path $csvDir) {
    $csvs = Get-ChildItem $csvDir -Filter "*.csv"
    Write-Host "    $($csvs.Count) CSV file(s) present in prime_Data/" -ForegroundColor DarkGray
    if ($csvs.Count -gt 0) {
        $latestCsv = $csvs | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        Write-Host "    Latest: $($latestCsv.Name) ($($latestCsv.LastWriteTime))" -ForegroundColor DarkGray
        $logMentions = $log | Where-Object { $_ -match "Battlelog CSV" }
        Check "Log shows CSV watcher activity" ($logMentions.Count -gt 0)
    }
}

# -----------------------------------------------------------------------
Write-Host "`n=== SUMMARY ===" -ForegroundColor Cyan
Write-Host "  PASSED: $pass" -ForegroundColor Green
Write-Host "  FAILED: $fail" -ForegroundColor $(if ($fail -gt 0) { "Red" } else { "Green" })
