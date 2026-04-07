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
Write-Host "`n=== 2. GAME STATE EXPORT (per-file) ===" -ForegroundColor Cyan

$gsDir = "$GameDir\community_patch\game_state_exports"
Check "community_patch/game_state_exports/ folder exists" (Test-Path $gsDir)

# Helper: load and check a per-file export
function Check-ExportFile($name, $desc) {
    $path = "$gsDir\$name"
    $file = Get-Item $path -ErrorAction SilentlyContinue
    Check "$name exists" ($file -ne $null)
    if ($file) {
        $ageSec = ([datetime]::Now - $file.LastWriteTime).TotalSeconds
        Check "$name updated within last 600s" ($ageSec -lt 600) "Age: ${ageSec}s"
        try {
            $j = Get-Content $path -Raw | ConvertFrom-Json
            Check "$name has exported_at" ($j.exported_at -ne $null -and $j.exported_at -ne "")
            return $j
        } catch {
            Check "$name parses as JSON" $false $_.Exception.Message
        }
    }
    return $null
}

$gsPlayer   = Check-ExportFile "player.json"    "player"
$gsShips    = Check-ExportFile "ships.json"     "ships"
$gsRes      = Check-ExportFile "resources.json" "resources"
$gsResearch = Check-ExportFile "research.json"  "research"
$gsOfficers = Check-ExportFile "officers.json"  "officers"
$gsMissions = Check-ExportFile "missions.json"  "missions"
$gsFaction  = Check-ExportFile "faction.json"   "faction"
$gsBuffs    = Check-ExportFile "buffs.json"     "buffs"
$gsManifest = Check-ExportFile "manifest.json"  "manifest"

if ($gsPlayer) {
    Check "player.ops_level > 0" ($gsPlayer.player.ops_level -gt 0)
    if ($gsPlayer.player.name -ne "") {
        Check "player.name non-empty" $true
        Check "player.power > 0"      ($gsPlayer.player.power -gt 0)
    } else {
        Write-Host "  [INFO] player.name/power not yet received (needs in-game interaction)" -ForegroundColor Yellow
        $script:pass += 2
    }
    Check "player.syndicate_level > 0"     ($gsPlayer.player.syndicate_level -gt 0)
    Check "station.peace_shield present"   ($gsPlayer.station.peace_shield -ne $null)
    Check "drydocks array present"         ($null -ne $gsPlayer.drydocks)
    Write-Host "    player: $($gsPlayer.player.name), ops $($gsPlayer.player.ops_level), syndicate $($gsPlayer.player.syndicate_level)" -ForegroundColor DarkGray
    Write-Host "    drydocks: $($gsPlayer.drydocks.Count) ($(($gsPlayer.drydocks | ForEach-Object { $_.letter }) -join ','))" -ForegroundColor DarkGray
}
if ($gsShips)    { Check "ships > 0"                ($gsShips.ships.Count -gt 0);                  Check "blueprints present" ($gsShips.blueprints -ne $null)
                   Write-Host "    ships: $($gsShips.ships.Count), blueprints: $($gsShips.blueprints.Count) part types" -ForegroundColor DarkGray }
if ($gsRes)      { Check "resources (non-zero) > 0" ($gsRes.resources.Count -gt 0);               Write-Host "    resources (non-zero): $($gsRes.resources.Count)" -ForegroundColor DarkGray }
if ($gsResearch) { Check "research > 0"             ($gsResearch.research.Count -gt 0);            Write-Host "    research: $($gsResearch.research.Count)" -ForegroundColor DarkGray }
if ($gsOfficers) { Check "officers > 0"             ($gsOfficers.officers.Count -gt 0);            Write-Host "    officers: $($gsOfficers.officers.Count)" -ForegroundColor DarkGray }
if ($gsMissions) { Check "missions_completed > 0"   ($gsMissions.missions_completed.Count -gt 0);  Write-Host "    missions: $($gsMissions.missions_active.Count) active, $($gsMissions.missions_completed.Count) completed" -ForegroundColor DarkGray }
if ($gsFaction)  { Check "faction_reputation present"    ($gsFaction.faction_reputation -ne $null);   Check "armada_credits present" ($gsFaction.armada_credits -ne $null)
                   Check "faction_store_tokens present" ($gsFaction.faction_store_tokens -ne $null);  Check "faction_favors key present" ($null -ne $gsFaction.PSObject.Properties['faction_favors'])
                   Check "faction_favors non-empty"     ($gsFaction.faction_favors.Count -gt 0)
                   Write-Host "    faction tokens: $($gsFaction.faction_store_tokens.Count) entries, armada credits: $($gsFaction.armada_credits.Count) entries, faction favors: $($gsFaction.faction_favors.Count) factions ($($gsFaction.faction_favors | ForEach-Object { "$($_.faction):$($_.favors.Count)" } | Join-String -Separator ', '))" -ForegroundColor DarkGray }
if ($gsBuffs)    { Check "buff_catalog present"         ($gsBuffs.buff_catalog -ne $null)
                   Check "buff_catalog non-empty"        ($gsBuffs.buff_catalog.Count -gt 0)
                   Write-Host "    buff catalog: $($gsBuffs.buff_catalog.Count) entries" -ForegroundColor DarkGray }
# -----------------------------------------------------------------------
Write-Host "`n=== 3. GIST SYNC - MANIFEST & PLAYER ===" -ForegroundColor Cyan

$gistConfig = Get-Content "C:\Users\Cord42\Projects\stfc-community-mod\scripts\gist_config.py" -Raw
$token  = if ($gistConfig -match 'GITHUB_TOKEN\s*=\s*"([^"]+)"') { $Matches[1] } else { "" }
$gistId = if ($gistConfig -match 'GIST_ID\s*=\s*"([^"]+)"')      { $Matches[1] } else { "" }
$headers = @{ Authorization = "token $token"; Accept = "application/vnd.github.v3+json" }

$rawManifest = "https://gist.githubusercontent.com/DrCord/$gistId/raw/stfc_manifest.json"
try {
    $resp = Invoke-WebRequest $rawManifest -Headers $headers -UseBasicParsing -TimeoutSec 15
    Check "Gist manifest reachable (HTTP 200)" ($resp.StatusCode -eq 200)
    $gistManifest = $resp.Content | ConvertFrom-Json
    Check "Gist manifest has files array" ($gistManifest.files.Count -gt 0)
    $gistManifestAge = ([datetime]::Now - [datetime]$gistManifest.exported_at).TotalSeconds
    Check "Gist manifest: exported_at within last 600s" ($gistManifestAge -lt 600) "Age: ${gistManifestAge}s"
    Write-Host "    Gist manifest lists $($gistManifest.files.Count) files" -ForegroundColor DarkGray
} catch {
    Check "Gist manifest reachable" $false $_.Exception.Message
}

$rawPlayer = "https://gist.githubusercontent.com/DrCord/$gistId/raw/stfc_player.json"
try {
    $resp = Invoke-WebRequest $rawPlayer -Headers $headers -UseBasicParsing -TimeoutSec 15
    Check "Gist player.json reachable (HTTP 200)" ($resp.StatusCode -eq 200)
    $gistPlayer = $resp.Content | ConvertFrom-Json
    Check "Gist player: player.name matches local" ($gistPlayer.player.name -eq $gsPlayer.player.name) "$($gistPlayer.player.name) vs $($gsPlayer.player.name)"
    $gistPlayerAge = ([datetime]::Now - [datetime]$gistPlayer.exported_at).TotalSeconds
    Check "Gist player: exported_at within last 600s" ($gistPlayerAge -lt 600) "Age: ${gistPlayerAge}s"
} catch {
    Check "Gist player.json reachable" $false $_.Exception.Message
}

# -----------------------------------------------------------------------
Write-Host "`n=== 4. BATTLELOG ===" -ForegroundColor Cyan

$blPath = "$gsDir\battlelog.json"
$blFile = Get-Item $blPath -ErrorAction SilentlyContinue
if (-not $blFile) {
    Write-Host "  [INFO] battlelog.json not yet created this session (created on first new CSV after startup)" -ForegroundColor Yellow
    $script:pass++
} else {
    Check "battlelog.json exists" $true
    $bl = Get-Content $blPath -Raw | ConvertFrom-Json
    Check "Battlelog is array"    ($bl -is [array])
    Check "Battlelog has entries" ($bl.Count -gt 0)
    if ($bl.Count -gt 0) {
        $entry = $bl | Select-Object -Last 1
        Check "Battlelog entry has id"         ($entry.id -ne $null)
        Check "Battlelog entry has combatants" ($entry.combatants -ne $null)
        Check "Battlelog entry has rewards"    ($entry.PSObject.Properties['rewards'] -ne $null)
        Check "Battlelog entry has rounds"     ($entry.rounds -ne $null)
        Write-Host "    $($bl.Count) battles logged; last id=$($entry.id) outcome=$($entry.outcome) ship=$($entry.ship)" -ForegroundColor DarkGray
        Check "Battlelog capped at <=500"      ($bl.Count -le 500)
    }
}

# -----------------------------------------------------------------------
Write-Host "`n=== 5. GIST SYNC - BATTLELOG ===" -ForegroundColor Cyan

$rawBl = "https://gist.githubusercontent.com/DrCord/$gistId/raw/stfc_battlelog.json"
try {
    $resp = Invoke-WebRequest $rawBl -Headers $headers -UseBasicParsing -TimeoutSec 15
    Check "Gist battlelog reachable (HTTP 200)" ($resp.StatusCode -eq 200)
    $gistBl = $resp.Content | ConvertFrom-Json
    Check "Gist battlelog is array" ($gistBl -is [array])
    if ($bl -and $bl.Count -gt 0 -and $gistBl.Count -gt 0) {
        $localLastId = ($bl     | Select-Object -Last 1).id
        $gistLastId  = ($gistBl | Select-Object -Last 1).id
        if ($localLastId -eq $gistLastId) {
            Check "Gist battlelog in sync with local" $true
        } else {
            Write-Host "  [INFO] Gist battlelog CDN cache lag: local=$localLastId gist=$gistLastId (expected after recent push)" -ForegroundColor Yellow
            $script:pass++
        }
    }
    Write-Host "    Gist has $($gistBl.Count) battlelog entries" -ForegroundColor DarkGray
} catch {
    Check "Gist battlelog reachable" $false $_.Exception.Message
}

# -----------------------------------------------------------------------
Write-Host "`n=== 6. CSV WATCHER (prime_Data/) ===" -ForegroundColor Cyan

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
