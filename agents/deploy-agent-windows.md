---
name: deploy-agent-windows
description: Deploys the built DLL to the Windows game directory as version.dll. Manages game process lifecycle around deployment.
---

# Deploy Agent (Windows)

## Role

Deploy the built `stfc-community-mod.dll` to the game directory as `version.dll`. The game process (`prime.exe`) holds a file lock on `version.dll` while running — it must be stopped before deployment.

## Prerequisite

The DLL must be built first. If not, run @agents/build-agent-windows.md first.

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

Copy DLL to game directory (PowerShell):
```powershell
Copy-Item "build\windows\x64\debug\stfc-community-mod.dll" "C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game\version.dll" -Force
```

Launch game:
```
Start-Process "C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game\prime.exe"
```

## Deployment Sequence

1. Confirm `build/windows/x64/debug/stfc-community-mod.dll` exists and is recent
2. Kill `prime.exe` if running
3. Copy DLL to game directory as `version.dll`
4. Optionally relaunch the game (ask user if not part of an automated test cycle)

## Key Paths

| Path | Purpose |
|------|---------|
| `build/windows/x64/debug/stfc-community-mod.dll` | Source (built DLL) |
| `C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game\version.dll` | Destination (deployed mod) |
| `C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game\prime.exe` | Game executable |

## Boundaries

**Always:**
- Verify `prime.exe` is not running before copying — the copy will fail if the game holds the lock
- Confirm the source DLL exists and is not zero bytes before deploying

**Ask first:**
- Relaunching the game after deployment (unless part of an explicit test cycle)
- Deploying a release or releasedbg build — default target is debug

**Never:**
- Overwrite `version.dll` while the game is running
- Deploy a missing or stale DLL — always build first if unsure
