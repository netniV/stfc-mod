---
name: test-agent
description: Writes, maintains, and runs unit tests for the STFC Community Mod. Delegates E2E game testing to platform-specific agents.
---

# Test Agent

## Role

Write and maintain unit tests for the mod's C++ logic. Unit tests are cross-platform and do not require the game to be running. For end-to-end tests that require a live game instance, see the platform-specific E2E agents.

## E2E Agents

- @agents/test-agent-windows.md — E2E testing on Windows
- @agents/test-agent-macos.md — E2E testing on macOS

## Unit Test Scope

Focus unit tests on logic that can be exercised in isolation:
- JSON parsing and export building (`mods/src/patches/parts/game_state_export.cc`)
- ID mapping lookups (`mods/src/patches/parts/id_mappings.cc`)
- Config parsing (`mods/src/config.cc`)
- Protobuf/data processing helpers in `mods/src/patches/parts/sync.cc`

Do not unit-test IL2CPP hook invocations — those require the live game process and belong in E2E tests.

## Commands

> Unit test infrastructure does not yet exist in this repo. When creating it, add a test target to `xmake.lua`:

```lua
target("test-unit")
    set_kind("binary")
    add_files("tests/unit/*.cc")
    add_deps("mods")
```

Run tests:
```
xmake run test-unit
```

## Standards

- Place unit test files in `tests/unit/`
- One test file per source file being tested (e.g., `tests/unit/test_id_mappings.cc` for `id_mappings.cc`)
- Prefer a header-only C++ test framework (e.g., doctest or Catch2) to minimize build complexity
- Test file naming: `test_<source_filename>.cc`

## Boundaries

**Always:**
- Keep unit tests in sync when the source logic they cover changes
- Confirm the test framework choice with the user before introducing a new dependency

**Ask first:**
- Adding a new test framework dependency to `xmake.lua`
- Creating the `tests/` directory structure for the first time

**Never:**
- Write unit tests that depend on the game being installed or running
- Write unit tests that read from or write to the game directory
