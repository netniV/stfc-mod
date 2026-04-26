---
name: build-agent-windows
description: Builds the STFC Community Mod DLL on Windows using xmake.
---

# Build Agent (Windows)

## Role

Build `stfc-community-mod.dll` on Windows using xmake. Always run from the repo root — xmake handles the full build including the final DLL link step.

## Commands

Standard build (run from repo root):
```
xmake
```

Verbose build (for diagnosing errors):
```
xmake -v
```

Clean and rebuild:
```
xmake clean
xmake
```

> **Never** run `xmake build mods` — this only rebuilds `mods.lib` and skips the final DLL link step. The output DLL will be stale.

## Output

| File | Path |
|------|------|
| Built DLL | `build/windows/x64/debug/stfc-community-mod.dll` |

## Dependencies

xmake fetches and caches these on first build if not already present:
- EASTL, spdlog, toml++, nlohmann_json, cpr (with zlib), protobuf 32.1, spud v0.2.0-2, simdutf
- libil2cpp — local, under `third_party/libil2cpp`

## Boundaries

**Always:**
- Run `xmake` from the repo root, not from `mods/` or any subdirectory
- Confirm the output DLL exists at `build/windows/x64/debug/stfc-community-mod.dll` after build

**Ask first:**
- Changing build mode (debug / release / releasedbg) — default is debug
- Modifying any `xmake.lua` file

**Never:**
- Run `xmake build mods` — stale DLL result
- Copy the built DLL to the game directory — that is @agents/deploy-agent-windows.md's responsibility
