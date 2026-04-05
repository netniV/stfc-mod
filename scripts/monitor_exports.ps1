# Monitor for ships and officers data in exports

$gamePath = "C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game"
$fullFile = Join-Path $gamePath "community_patch_gamestate.json"
$deltaFile = Join-Path $gamePath "community_patch_gamestate_delta.json"

Write-Host "?? Monitoring gamestate exports for ships and officers data..." -ForegroundColor Cyan
Write-Host "?? Watching: $fullFile" -ForegroundColor Gray
Write-Host "??  Checking every 10 seconds... Press Ctrl+C to stop`n" -ForegroundColor Gray

$lastFullTime = (Get-Item $fullFile).LastWriteTime
$lastDeltaTime = (Get-Item $deltaFile).LastWriteTime

while ($true) {
    Start-Sleep -Seconds 10

    $currentFullTime = (Get-Item $fullFile).LastWriteTime
    $currentDeltaTime = (Get-Item $deltaFile).LastWriteTime

    # Check if full export updated
    if ($currentFullTime -ne $lastFullTime) {
        Write-Host "`n? NEW FULL EXPORT detected at $($currentFullTime.ToString('HH:mm:ss'))" -ForegroundColor Green

        $gs = Get-Content $fullFile | ConvertFrom-Json
        Write-Host "   Ships: $($gs.ships.Count)" -ForegroundColor $(if ($gs.ships.Count -gt 0) { "Green" } else { "Yellow" })
        Write-Host "   Officers: $($gs.officers.Count)" -ForegroundColor $(if ($gs.officers.Count -gt 0) { "Green" } else { "Yellow" })
        Write-Host "   Buildings: $($gs.buildings.Count)" -ForegroundColor $(if ($gs.buildings.Count -gt 0) { "Green" } else { "Yellow" })
        Write-Host "   Research: $($gs.research.Count)" -ForegroundColor $(if ($gs.research.Count -gt 0) { "Green" } else { "Yellow" })

        if ($gs.ships.Count -gt 0 -and $gs.officers.Count -gt 0) {
            Write-Host "`n?? SUCCESS! Ships and officers data found!" -ForegroundColor Green
            Write-Host "   Run: python scripts\data_extraction\extract_my_data.py" -ForegroundColor Cyan
            break
        }

        $lastFullTime = $currentFullTime
    }

    # Check if delta updated
    if ($currentDeltaTime -ne $lastDeltaTime) {
        $delta = Get-Content $deltaFile | ConvertFrom-Json
        $latest = $delta[-1]
        Write-Host "?? New differential at $($latest.exported_at)" -ForegroundColor Gray
        Write-Host "   Categories: $($latest.changes.PSObject.Properties.Name -join ', ')" -ForegroundColor Gray

        $lastDeltaTime = $currentDeltaTime
    }

    Write-Host "." -NoNewline -ForegroundColor DarkGray
}
