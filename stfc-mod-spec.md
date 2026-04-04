# STFC Community Mod — Feature Development Specification

**Project:** `stfc-community-mod` (fork of `netniV/stfc-mod`)
**Repo:** `https://github.com/DrCord/stfc-community-mod`
**Branch:** `dev` (always work here, never `main`)
**Language:** C++ (MSVC, C++17)
**Build system:** XMake → generates VS2022 solution via `xmake project -k vsxmake2022`
**Game:** Star Trek Fleet Command (Windows PC client, Scopely)
**Player:** DrCord — Ops 41, Server 709, Alliance [GROW]

---

## How the Mod Works

The mod is a Windows proxy DLL (`version.dll`) placed in the STFC game directory:
```
C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game\version.dll
```

When the game launches it loads `version.dll`, which hooks into the game process using IL2CPP
function interception. The mod reads game data from memory in real time and can intercept
and modify game behavior. Configuration is via a TOML file in the same directory.

### Key source files (all under `mods/src/`)

| File | Purpose |
|------|---------|
| `config.cc` | TOML config loading — add new settings here |
| `file.cc` | File I/O utilities — use for writing export files |
| `patches/patches.cc` | Entry point that wires up all patch modules |
| `patches/parts/sync.cc` | Existing data sync/export code — primary reference for new features |
| `patches/parts/object_tracker.cc` | Tracks game objects in memory — source of game state data |
| `patches/parts/hotkeys.cc` | Keyboard shortcut handling |
| `patches/parts/zoom.cc` | Zoom functionality (recently broken by game update, now fixed in dev) |
| `il2cpp/il2cpp-functions.cc` | IL2CPP function bindings — game internals access |
| `il2cpp/il2cpp_helper.cc` | Helper utilities for IL2CPP interop |
| `prime/proto/*.proto` | Protobuf definitions for game data structures |

### Key protobuf data structures available

| Proto file | Contains |
|-----------|---------|
| `Digit.Prime.Inventories.proto` | Player inventory, resources, items |
| `Digit.Prime.Missions.proto` | Mission/away team data |
| `Digit.Prime.TerritoryCapture.proto` | Territory capture events |
| `Digit.Prime.PersistentPrefs.proto` | Player preferences/settings |
| `Digit.Client.SaveSystem.proto` | Save system data (buildings, research, ships) |
| `stfc.proto` | Core STFC game data types |

---

## Features to Build

### Feature 1: Game State JSON Export ⭐ HIGHEST PRIORITY

**Goal:** Export a structured JSON snapshot of the player's current game state to a file on disk
while the game is running. This enables external tools (Python scripts, Claude AI) to read
current game data without manual CSV exports.

**Output file:** `community_patch_gamestate.json` (same directory as the DLL)

**Data to include:**
```json
{
  "exported_at": "2026-04-03T09:00:00Z",
  "player": {
    "ops_level": 41,
    "server": 709
  },
  "buildings": [
    { "id": 0, "name": "Operations", "level": 41 },
    { "id": 72, "name": "Mess Hall", "level": 46 }
  ],
  "research": [
    { "id": 12345, "name": "...", "level": 5, "max_level": 10 }
  ],
  "ships": [
    { "id": "defiant", "tier": 4, "level": 20 }
  ],
  "faction_rep": [
    { "faction": "federation", "points": 25258867 }
  ],
  "resources": {
    "parsteel": 0,
    "tritanium": 0,
    "dilithium": 0
  },
  "bp_progress": [
    { "ship": "enterprise", "current": 133, "required": 150 }
  ]
}
```

**Implementation approach:**
- Add a configurable export interval to `community_patch_settings.toml` (default: 300 seconds)
- Hook into the existing sync/object_tracker infrastructure in `sync.cc` and `object_tracker.cc`
- Use `file.cc` utilities for writing the JSON file
- Use `nlohmann_json` (already a project dependency) for JSON serialization
- Trigger export on a timer and also on game close

**Config entry to add:**
```toml
# Game state JSON export
export_gamestate = true
export_gamestate_interval = 300  # seconds, 0 = on close only
export_gamestate_path = ""  # empty = same dir as DLL
```

---

### Feature 2: Enhanced Battle Log Export

**Goal:** Export structured JSON battle logs so combat history can be analyzed externally.
The mod already has some battlelog syncing — this expands it to a full structured export.

**Output file:** `community_patch_battlelog.json`

**Data per battle entry:**
```json
{
  "timestamp": "2026-04-03T09:15:00Z",
  "attacker": "DrCord",
  "defender": "FromTheEmbers",
  "attacker_ship": "Defiant",
  "defender_ship": "unknown",
  "outcome": "win",
  "system": "Romulan Space",
  "damage_dealt": 125000,
  "damage_received": 45000
}
```

**Implementation approach:**
- Extend existing battlelog sync code in `sync.cc`
- Append new entries to the JSON file rather than overwriting
- Cap file size at last 500 battles

---

### Feature 3: Alliance Discord Webhook — Territory Reminders

**Goal:** Post territory capture reminders to a Discord channel webhook automatically
when a territory capture event is imminent.

**Note:** This feature is partially implemented externally via Cloudflare Workers
(see project `stfc-discord-tc-notifications`). The mod version would be more accurate
since it can read actual in-game TC timer data directly.

**Config entries:**
```toml
discord_webhook_url = ""
discord_tc_reminder_minutes = 15  # how many minutes before TC to post
```

**Implementation approach:**
- Use `libcurl` (already a project dependency via `cpr`) to POST to Discord webhook
- Read territory capture timer from `Digit.Prime.TerritoryCapture.proto` data
- Post an embed message with territory name, time remaining, and checklist

---

### Feature 4: Resource and Shield Alerts

**Goal:** Show an in-game overlay or log warning when:
- Resources exceed protected cargo (vulnerability to attack)
- Shield is down and resources are above protected cargo threshold
- Repair timer exceeds a configured threshold

**Config entries:**
```toml
alert_unprotected_resources = true
alert_shield_down_threshold = 0  # 0 = always alert when unprotected
alert_repair_timer_hours = 2
```

---

## Development Workflow

### Build cycle
```
1. Edit source files in VS2022
2. Ctrl+Shift+B to build
3. copy build\windows\x64\debug\stfc-community-mod.dll
       "C:\Games\Star Trek Fleet Command\...\default\game\version.dll"
4. Launch STFC and test
5. Check community_patch.log for debug output
```

### Adding a new feature module

1. Create `mods/src/patches/parts/your_feature.cc` and `.h`
2. Add to `mods/src/patches/patches.cc` — call your init function from `init_patches()`
3. Add config entries to `config.cc` and `community_patch_settings.toml`
4. Use `spdlog` for logging (already wired up): `spdlog::info("message {}", value);`

### Logging
The mod writes to `community_patch.log` in the game directory. Use spdlog:
```cpp
#include <spdlog/spdlog.h>
spdlog::info("GameState export: wrote {} bytes to {}", bytes, path);
spdlog::warn("GameState export: could not find inventory data");
spdlog::error("GameState export: file write failed: {}", ec.message());
```

### JSON output (nlohmann_json)
```cpp
#include <nlohmann/json.hpp>
using json = nlohmann::json;

json j;
j["player"]["ops_level"] = 41;
j["buildings"] = json::array();
j["buildings"].push_back({ {"id", 0}, {"name", "Operations"}, {"level", 41} });

std::string output = j.dump(2);  // pretty print with 2-space indent
```

### File writing (use file.cc utilities or standard C++)
```cpp
#include <fstream>
std::ofstream out(export_path);
out << j.dump(2);
out.close();
```

---

## PR Strategy

All features are being developed on the `dev` branch of `DrCord/stfc-community-mod`
(forked from `netniV/stfc-mod`). Once features are stable and tested:

1. Open a PR from `DrCord/stfc-community-mod:dev` → `netniV/stfc-mod:dev`
2. Each feature should be a separate PR for easier review
3. Feature 1 (JSON export) is the highest value contribution for the community
4. Discuss in mod Discord (`discord.gg/PrpHgs7Vjs`) before opening PRs

---

## Dependencies Available (no additional installs needed)

| Library | Version | Use |
|---------|---------|-----|
| `nlohmann_json` | v3.12.0 | JSON serialization ✅ |
| `spdlog` | v1.17.0 | Logging ✅ |
| `toml++` | v3.4.0 | Config file parsing ✅ |
| `libcurl` / `cpr` | 8.11.0 / 1.14.2 | HTTP requests (Discord webhook) ✅ |
| `protobuf` | 32.1 | Game data deserialization ✅ |
| `abseil` | 20250512.1 | Utility library (protobuf dependency) ✅ |

---

## Important Notes

- **Never commit directly to `main`** — always work on `dev`
- **GPL-3.0 license** — all contributions must be open source
- **Game ToS awareness** — stay within the spirit of existing mod features
- **The zoom feature** was recently broken by a Scopely game update and fixed in `dev` —
  this is why we're on `dev` and not `main`
- **Debug builds** are fine for development; release builds for distribution
- **The mod Discord** (`discord.gg/PrpHgs7Vjs`) is the right place to coordinate
  with netniV before opening PRs

---

## Context: Why We're Building This

The primary driver is enabling AI-assisted game planning. By exporting game state to JSON,
a Python script can automatically push it to a GitHub Gist, which Claude AI can then fetch
at the start of any planning conversation — eliminating the need to manually upload
screenshots and CSV exports to stay up to date.

Secondary drivers: battle log analysis (tracking attacker `FromTheEmbers`),
alliance coordination (territory capture reminders), and general QoL improvements
for the STFC community.
