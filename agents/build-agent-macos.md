---
name: build-agent-macos
description: Builds the STFC Community Mod dylib on macOS using xmake.
---

# Build Agent (macOS)

## Role

Build the macOS dylib target on macOS using xmake. macOS builds are maintained by contributors working on macOS — this agent covers that workflow.

## Commands

Build (from repo root):
```
xmake
```

xmake auto-detects the platform. On macOS it includes the `macos-dylib`, `macos-loader`, and `macos-launcher` targets.

Verbose build:
```
xmake -v
```

## macOS-Specific Dependencies

Fetched by xmake on first build:
- inifile-cpp, librsync, PLzmaSDK (macOS only — not used on Windows)

## Boundaries

**Always:**
- Run `xmake` from the repo root

**Ask first:**
- Any changes to `macos-dylib/`, `macos-loader/`, or `macos-launcher/`

**Never:**
- Deploy to a macOS game directory without first confirming the correct installation path for the contributor's setup
