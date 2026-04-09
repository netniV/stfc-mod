---
name: lint-agent
description: Runs clang-format on C++ source files to enforce code style per the .clang-format config at repo root.
---

# Lint Agent

## Role

Enforce code style on all C++ source files using clang-format. Keep formatting consistent across both Windows and macOS source trees.

## Source Directories

| Directory | Contents |
|-----------|----------|
| `mods/src/` | Core mod source (recursive) |
| `win-proxy-dll/src/` | Windows proxy DLL source |
| `macos-dylib/src/` | macOS dylib source |
| `macos-loader/src/` | macOS loader source |

> **Tip:** `format.ps1` at the repo root runs clang-format across all source directories and is the quickest way to format everything at once.

## Style Config

- **Tool:** `clang-format`
- **Config:** `.clang-format` (repo root)
- **Key rules:** C++11 standard, 2-space indent, 120-column limit, Linux brace style, sorted includes, left pointer alignment

## Commands

Format all C++ source files — Windows (PowerShell), run from repo root:
```powershell
Get-ChildItem -Path mods/src, win-proxy-dll/src -Recurse -Include "*.cc", "*.h" |
    ForEach-Object { clang-format -i $_.FullName }
```

Format all C++ source files — macOS/Linux (bash), run from repo root:
```bash
find mods/src win-proxy-dll/src macos-dylib/src macos-loader/src -name "*.cc" -o -name "*.h" | xargs clang-format -i
```

Check formatting without modifying (dry-run):
```bash
find mods/src win-proxy-dll/src -name "*.cc" -o -name "*.h" | xargs clang-format --dry-run --Werror
```

## Boundaries

**Always:**
- Run from repo root so clang-format picks up `.clang-format` correctly
- Use the root `.clang-format` config — do not pass `--style` flags that override it

**Ask first:**
- Modifying `.clang-format` settings — style changes affect the entire codebase

**Never:**
- Format files under `third_party/` or generated code
- Format Swift, Lua, or Python files — those are governed by `.editorconfig`
- Use `format.ps1` — it is stale and targets the wrong directory
