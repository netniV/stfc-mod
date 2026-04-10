---
name: stfc-community-mod
description: AI agent instructions for the STFC Community Mod — a Windows proxy DLL that hooks Star Trek Fleet Command to export real-time game state as JSON.
---

# STFC Community Mod — Agent Instructions

## Project Overview

A Windows proxy DLL (`version.dll`) that hooks Star Trek Fleet Command (PC/Scopely) via IL2CPP interception. It reads game data from memory in real-time and exports structured JSON snapshots to local files and a GitHub Gist for use by external tools and AI assistants.

**Player:** SpotTheSpaceCat · Ops 41 · Server 709 · Alliance [GROW]

> For AI assistants helping the player with in-game planning, see
> [`docs/reference/ai-assistant-context.md`](docs/reference/ai-assistant-context.md).

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Language | C++23 |
| Build system | xmake |
| Formatter | clang-format (config: `.clang-format` at repo root) |
| Hook mechanism | IL2CPP interception via `mods/src/il2cpp/` |
| Key libraries | EASTL, spdlog, toml++, nlohmann_json, cpr, protobuf 32.1 |
| Scripting | Python 3 (`scripts/`), PowerShell (`scripts/*.ps1`) |

## Key File Paths

### Source

| Path | Contents |
|------|----------|
| `mods/src/patches/parts/` | Core patches — `game_state_export.cc`, `sync.cc`, etc. |
| `mods/src/il2cpp/` | IL2CPP interception helpers |
| `win-proxy-dll/src/` | Windows proxy DLL entry point |
| `scripts/` | Python and PowerShell utility scripts |
| `docs/` | Design docs and implementation notes |

### Windows Runtime

| Path | Contents |
|------|----------|
| `build/windows/x64/debug/stfc-community-mod.dll` | Build output |
| `C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game\` | Game installation directory |
| `C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game\prime.exe` | Game executable |
| `C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game\version.dll` | Deployed mod (overwrite to update) |
| `C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game\community_patch.log` | Mod runtime log |
| `C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game\community_patch\game_state_exports\` | JSON export output |
| `C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game\community_patch_settings.toml` | Mod config |

## Deployment (Windows)

`prime.exe` holds a file lock on `version.dll` while running. Deployment sequence:

1. Kill game: `taskkill.exe //F //IM prime.exe`
2. Copy `build/windows/x64/debug/stfc-community-mod.dll` → game dir as `version.dll`
3. Relaunch: `Start-Process "C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game\prime.exe"`

> Use `//F` not `/F` for taskkill — Git Bash mangles single-slash flags as paths.

## Shell / Bash Command Guidelines

- **Issue commands one at a time.** Do not chain commands with `&&`.
- Chained commands require individual authorization on each run, cannot be pre-authorized, and cannot be individually rerun.
- Use absolute paths when possible to avoid needing `cd` calls.
- Git CLI is available — use it directly.

## Git Workflow

- Active branch: `feature/sync_game_state` — all development work goes here
- Never commit directly to `main`
- Never commit the `.claude/` directory
- Always co-author AI-assisted commits: `Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>`

## Game File Access Policy

All agents have read-only access to the game directory by default. Write and execute access is granted only to agents that explicitly require it:

| Agent | Game dir access |
|-------|----------------|
| All agents (default) | Read-only |
| @agents/deploy-agent-windows.md | Read + write + execute (`version.dll`, `prime.exe`) |
| @agents/cleanup-agent.md | Read + write (exports dir and log only) |

## Agents

| Agent | Scope |
|-------|-------|
| @agents/docs-agent.md | Documentation — all platforms |
| @agents/lint-agent.md | Code formatting and style — all platforms |
| @agents/build-agent-windows.md | Build (Windows) |
| @agents/build-agent-macos.md | Build (macOS) |
| @agents/test-agent.md | Unit tests — cross-platform |
| @agents/test-agent-windows.md | E2E tests (Windows) |
| @agents/test-agent-macos.md | E2E tests (macOS) |
| @agents/deploy-agent-windows.md | Deploy to game directory (Windows) |
| @agents/cleanup-agent.md | Clean game-side artifacts (Windows) |

## Universal Constraints

**Always:**
- Run `xmake` from the repo root (not from subdirectories)
- Confirm the game is not running before writing to the game directory
- Issue one shell command at a time — no `&&` chaining

**Ask first:**
- Any change that writes outside the repo directory
- Pushing to a remote branch

**Never:**
- Commit secrets, API keys, `.env` files, or the `.claude/` directory
- Run `xmake build mods` — this only rebuilds `mods.lib` and leaves the final DLL stale
- Modify or delete `version.dll`, `prime.exe`, or `community_patch_settings.toml` in the game directory (deploy and cleanup agents have explicit narrow permissions above)
- Push directly to `main`
