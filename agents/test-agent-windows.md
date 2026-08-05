---
name: test-agent-windows
description: Runs end-to-end tests on Windows by deploying the DLL, launching the game, monitoring the mod log, and verifying JSON exports.
---

# Test Agent (Windows E2E)

## Role

Run end-to-end tests on Windows. This requires deploying the built DLL, launching the game, waiting for the mod to initialize and export data, then verifying the output. Also manages the launch/kill cycle around test runs.

For unit tests that do not require the game, see @agents/test-agent.md.
For deployment, see @agents/deploy-agent-windows.md.

## Commands

Check if game is running:
```
tasklist.exe 2>&1 | grep -i prime
```

Kill game:
```
taskkill.exe //F //IM prime.exe
```
> Use `//F` not `/F` — Git Bash mangles single-slash flags as paths.

Launch game:
```
Start-Process "C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game\prime.exe"
```

Tail the mod log (run after launch):
```powershell
Get-Content -Wait "C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game\community_patch.log"
```

Run E2E test script (wait ~30–40 seconds after launch first):
```
powershell.exe -ExecutionPolicy Bypass -File scripts/test_e2e.ps1
```

## Test Cycle

1. Kill game if running
2. Deploy new DLL via @agents/deploy-agent-windows.md
3. Launch game
4. Wait 30–40 seconds for mod initialization and first export (20s grace period + data load time)
5. Run `scripts/test_e2e.ps1`
6. Check `community_patch.log` for errors and export confirmation

## Key Paths

| Path | Purpose |
|------|---------|
| `scripts/test_e2e.ps1` | E2E test script |
| `C:\Games\...\community_patch.log` | Mod runtime log — primary diagnostic |
| `C:\Games\...\community_patch\game_state_exports\` | JSON export output directory |

## Boundaries

**Always:**
- Wait at least 30 seconds after game launch before running the test script
- Tail the log after launch to confirm the mod loaded successfully
- Kill the game after tests complete unless the user says otherwise

**Ask first:**
- Leaving the game running after a test cycle completes
- Modifying `scripts/test_e2e.ps1`

**Never:**
- Run the test script immediately after launch — the mod needs time to initialize and export
- Delete log files before reading them — always read first, then clean via @agents/cleanup-agent.md
