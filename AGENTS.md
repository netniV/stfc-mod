# AGENTS.md

This file provides guidance to Codex (Codex.ai/code) when working with code in this repository.

## Project Overview

Community mod for Star Trek Fleet Command (STFC) — a desktop game that runs via Unity/IL2CPP. The mod hooks into the game's IL2CPP runtime to add QoL features (UI scaling, zoom controls, hotkeys, chat improvements, cargo viewers, data sync, etc.). Supports Windows (DLL proxy injection) and macOS (dylib injection).

## Build System

This project uses **XMake** (not CMake). All build configuration is in `xmake.lua` files. Language standard is C++23 with multi-threaded static runtime (`/MT`).

### Build Commands

```bash
# Configure and build (command line)
xmake                              # Build default target
xmake f -p macosx -a arm64 -m debug --target_minver=13.5   # Configure for macOS ARM debug
xmake f -p windows -m release         # Configure for Windows release

# Generate Visual Studio solution
xmake project -k vsxmake -m "debug,release"

# Clean
xmake clean -a

# macOS dev script (build, run, debug, crashlogs)
scripts/mac-build-test-debug.sh [build|run|debug|crashlogs] [-m debug|release|releasedbg]
```

### Build Modes
- `debug` — development
- `release` — production
- `releasedbg` — release with debug info, enables `_MODDBG` define

### Reset Build
Delete the `build/` folder to reset. Also delete `.vs/` for a full Visual Studio reset.

## Repository Expectations

- Keep changes scoped. Do not stage unrelated dirty files or generated artifacts unless the user explicitly asks.
- Before finishing C++ or patch work, run `git diff --check` and the narrowest relevant xmake build.
- For macOS core mod changes, use `xmake f -p macosx -a arm64 -m debug --target_minver=13.5 -y && xmake -y mods`.
- Review the final diff for risky hooks, platform guards, config default mismatches, and missing example config updates.
- If a subtree such as `macos-launcher/` needs specialized guidance, prefer a nested `AGENTS.md` near that code instead of overloading this root file.

## Architecture

### Build Targets (xmake.lua files)

| Target | Type | Platform | Description |
|---|---|---|---|
| `mods` | static lib | all | Core mod logic — patches, config, IL2CPP bindings |
| `stfc-community-mod` (win-proxy-dll) | shared DLL | Windows | Proxy DLL (`version.dll`) that loads into the game process |
| `stfc-community-mod` (macos-dylib) | shared dylib | macOS | Injected dylib equivalent |
| `stfc-community-mod-loader` | binary | macOS | Loader that injects the dylib into the game |
| `macOSLauncher` | Xcode app | macOS | Swift GUI launcher app |

### Source Layout

- **`mods/src/`** — Core mod code (the main codebase)
  - `config.h/.cc` — Singleton `Config` class, loads TOML settings, controls which patches are enabled
  - `patches/patches.cc` — Entry point: hooks `il2cpp_init`, then conditionally installs each patch
  - `patches/parts/` — Individual patch implementations (zoom, hotkeys, chat, UI scale, sync, etc.)
  - `patches/key.h`, `mapkey.h`, `modifierkey.h` — Keyboard input mapping system
  - `prime/` — Header-only IL2CPP type definitions mirroring the game's C# classes
  - `prime/proto/` — Protobuf definitions for game data sync
  - `il2cpp/` — IL2CPP helper functions for resolving methods, classes, and icalls at runtime
- **`win-proxy-dll/src/`** — Windows DLL proxy entry point
- **`macos-dylib/src/`** — macOS dylib entry point
- **`macos-loader/src/`** — macOS loader (finds game, injects dylib)
- **`macos-launcher/`** — Swift macOS GUI app
- **`third_party/libil2cpp/`** — IL2CPP SDK headers
- **`xmake-packages/`** — Custom xmake package definitions (e.g., `spud`)

### Key Patterns

**Hooking pattern** — All game function hooks use `spud` (function detour library) via `SPUD_STATIC_DETOUR`. Each hook function takes `auto original` as the first parameter to call through to the original:
```cpp
void SomeFunction_Hook(auto original, SomeClass* _this, ...) {
    // custom logic
    original(_this, ...);
}
```
macOS does not tolerate repeated hooks of the same function. If multiple features need to intercept the same game method, consolidate the behavior behind one detour or add platform guards instead of installing overlapping hooks.
Do not over-focus on hidden IL2CPP `MethodInfo*` parameters during drift repair; match the game-visible signature from `dump.cs` unless there is concrete runtime evidence that the hidden parameter is the issue.

**IL2CPP class resolution** — Game classes are resolved at runtime using helpers:
```cpp
static auto class_helper = il2cpp_get_class_helper("Assembly.Name", "Namespace", "ClassName");
static auto method = class_helper.GetMethodInfo("MethodName");
```

**Adding a new patch** — Create a `.cc` file in `mods/src/patches/parts/`, write an `InstallXxxHooks()` function, declare it in `patches.cc`, add a `bool installXxx` to `Config`, and register in the `patches[]` array in `patches.cc`. Patch toggles are only read from TOML in `_MODDBG` builds, so update both the `_MODDBG` config parsing path and the non-`_MODDBG` release defaults in `config.cc`.

**Config** — User settings are in TOML files. The `Config` singleton (`Config::Get()`) is loaded once during `il2cpp_init_hook`. Add new settings to `config.h`, add defaults in `defaultconfig.h`, and load them in `config.cc`. For user-facing settings, update `example_community_patch_settings.toml` unless the setting is intentionally internal.

### Dependencies (via xmake packages)

- `spud` — Function hooking/detour library
- `eastl` — EA's STL replacement
- `spdlog` — Logging
- `toml++` — TOML config parsing
- `nlohmann_json` — JSON handling
- `cpr` / `libcurl` — HTTP requests (for data sync)
- `protobuf` — Protocol buffers (game data)
- `simdutf` — UTF encoding
- `libil2cpp` — Local package pointing to `third_party/libil2cpp`

## Code Style

- Uses `.clang-format` — 2-space indent, 120 column limit, Linux brace style, aligned assignments/declarations
- Version is defined in `mods/src/version.h` (VERSION_MAJOR/MINOR/REVISION/PATCH)
- Prefer narrow platform guards such as `#if _WIN32`, `#if !_WIN32`, and `#if __APPLE__`; do not assume every non-Windows path is macOS.
- Logging via `spdlog::info()`, `spdlog::debug()`, etc.

## Branches

- `main` — stable releases
- `dev` — active development (PR target)
