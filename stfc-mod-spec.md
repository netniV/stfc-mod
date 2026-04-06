# STFC Community Mod — Feature Development Specification

**Project:** `stfc-community-mod` (fork of `netniV/stfc-mod`)
**Repo:** `https://github.com/DrCord/stfc-community-mod`
**Branch:** `dev` (always work here, never `main`)
**Language:** C++ (MSVC, C++17)
**Build system:** XMake → generates VS2022 solution via `xmake project -k vsxmake2022`
**Game:** Star Trek Fleet Command (Windows PC client, Scopely)
**Player:** `<your in-game name>` — Ops 41, Server 709, Alliance [GROW]

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

### Feature 1: Game State JSON Export ✅ IMPLEMENTED

**Goal:** Export a structured JSON snapshot of the player's current game state to a file on disk
while the game is running. This enables external tools (Python scripts, Claude AI) to read
current game data without manual CSV exports.

**Output files:**
- `community_patch_gamestate.json` — full snapshot
- `community_patch_gamestate_delta.json` — incremental changes since last full export

**Currently exported data:**
```json
{
  "export_type": "full",
  "export_version": "1.1.0",
  "exported_at": "2026-04-06T07:32:40Z",
  "meta": { "mod_version": "1.0.0.0", "mappings_loaded": true },
  "player": {
    "name": "YourPlayerName",
    "ops_level": 41,
    "power": 19714080,
    "server": 709,
    "alliance": "",
    "alliance_id": 0
  },
  "buildings": [ { "id": 0, "name": "OPERATIONS", "level": 41 } ],
  "research":  [ { "id": 12345, "name": "...", "level": 5 } ],
  "ships":     [ { "id": 111, "hull_id": 222, "name": "Defiant", "tier": 4, "level": 20 } ],
  "officers":  [ { "id": 1, "name": "Spock", "rank": 3, "level": 45, "traits": [
    { "id": 101, "name": "Logical", "ability_level": 3 }
  ]}],
  "resources": [ { "id": 1234, "name": "Parsteel", "amount": 1000000 } ],
  "faction_reputation": [ { "faction": "Romulan", "points": 11077219, "resource_id": 4135751670 } ],
  "blueprints": [ { "name": "Resource_Parts_Battleship_G3", "amount": 11513, "resource_id": 1779580172 } ]
}
```

**Known gaps (to be filled by future data triggers):**
- `player.alliance` — populates when alliance profile response arrives in session
- `officer.traits[].ability_level` — populates when `ActiveOfficerTraits` response arrives

**Config (`[gamestate_export]` in `community_patch_settings.toml`):**
```toml
[gamestate_export]
enabled = true
export_on_startup = true
interval = 300                  # seconds between periodic exports; 0 = on-demand only
path = 'C:\Games\...\game'      # where to write files; empty = game directory
player_id = 'y806e96e...'       # your userid for accurate player data matching
```

**Implementation:** `mods/src/patches/parts/gamestate_export.cc` + `sync.cc`

---

### Feature 1b: Gamestate Gist Sync ✅ IMPLEMENTED

**Goal:** Push the gamestate JSON to a GitHub Gist automatically whenever it updates,
so AI assistants and external tools can read it via a stable public URL — without needing
a separate Python sync script running in the background.

**Config (`[gamestate_export.gist]` in `community_patch_settings.toml`):**
```toml
[gamestate_export.gist]
enabled = true
gist_id = 'your_gist_id_here'
token = 'ghp_your_token_here'
filename_full  = 'stfc_gamestate_full.json'   # optional, these are defaults
filename_delta = 'stfc_gamestate_delta.json'
```

**Behaviour:**
- After every successful full or delta file write, PATCHes the corresponding Gist file via
  the GitHub Gist API (`PATCH https://api.github.com/gists/{gist_id}`)
- Uses `cpr` (already a dependency) for the HTTP call
- Token stored only in the local settings file (gitignored); never committed
- Logs the raw Gist URLs on startup for sharing with AI assistants

**Raw URLs (example):**
```
https://gist.githubusercontent.com/{user}/{gist_id}/raw/stfc_gamestate_full.json
https://gist.githubusercontent.com/{user}/{gist_id}/raw/stfc_gamestate_delta.json
```

**Implementation:** `mods/src/patches/parts/gamestate_export.cc` — `sync_to_gist()` helper

**Replaces:** `scripts/sync_to_gist.py` / `scripts/sync_to_gist_v2.py` (now removed)

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

### Feature 3: Alliance Discord Webhook — Territory Reminders ➡️ OUT OF SCOPE

**Note:** This feature belongs in the separate `stfc-discord-tc-notifications` repo
(`C:\Users\Cord42\Projects\stfc-discord-tc-notifications`), not here.
Removing from this project's backlog.

---

### Feature 4: Drydock Export + Peace Shield Warnings ✅ IMPLEMENTED

**Goal:**
1. Export drydock assignments (which ship is in each active drydock A–E) to the gamestate JSON
2. Export station peace shield state (active/expired, expiry time, token count) to the gamestate JSON
3. Log warnings when the peace shield is down or approaching expiry thresholds

**Out of scope (shelved):** ship cargo vs. vault protection warnings, repair timer warnings.

#### Gamestate JSON additions

**`drydocks` array** — one entry per assigned drydock:
```json
"drydocks": [
  { "drydock_id": 1, "letter": "A", "ship_id": 111, "ship_name": "Defiant" },
  { "drydock_id": 2, "letter": "B", "ship_id": 222, "ship_name": "Saladin" }
]
```
Drydock letter is derived from `drydock_id`: `1=A, 2=B, 3=C, 4=D, 5=E`.
Each ship entry in the `ships` array is also annotated with `"drydock": "A"` and `"drydock_id": 1`.

**`station.peace_shield` object:**
```json
"station": {
  "peace_shield": {
    "active": true,
    "token_count": 3,
    "expires_at": "2026-04-06T19:38:00Z",
    "expires_epoch": 1744918680,
    "seconds_remaining": 32280
  }
}
```
When the shield is down: `"active": false`, `"expires_at": null`, `"seconds_remaining": 0`.

#### Warning log output

Warnings are written via `spdlog::warn` to `community_patch.log`:
```
SHIELD ALERT: Station peace shield is DOWN. Tokens in inventory: 2
SHIELD ALERT: Station peace shield expires in 1h 47m (threshold: 2h)
```

#### Data sources

| Data | Source | EntityGroup type |
|------|--------|-----------------|
| Drydock assignments | `EntitySlots` / `EntitySlotsData` — `SLOTTYPE_FLEETPRESET` → `FleetPresetSlotParams.setups[]` → `drydockId + shipIds[0]` | `117` / `121` |
| Peace shield state | `PlayerInventories` — `INVENTORYITEMTYPE_INVENTORYSHIELD (103)` items; `expiryTime` = active shield, `count` = tokens in stock | `46` |

#### Config (`[resource_alerts]` in `community_patch_settings.toml`)

```toml
[resource_alerts]
enabled = true
poll_interval_seconds = 60        # how often to evaluate alert conditions
reminder_interval_minutes = 30    # minimum gap between repeated warnings of the same type
shield_warn_hours = "4,2,1"       # warn when shield expiry is within these many hours
```

| Key | Default | Description |
|-----|---------|-------------|
| `enabled` | `false` | Master switch for all resource/shield alerts |
| `poll_interval_seconds` | `60` | How often (seconds) the warning logic runs |
| `reminder_interval_minutes` | `30` | Minimum minutes between repeated warnings |
| `shield_warn_hours` | `"4,2,1"` | Comma-separated hours-before-expiry thresholds |

**Implementation:** `mods/src/patches/parts/gamestate_export.cc` — `capture_peace_shield()`, `capture_drydock_assignments()`, `check_shield_warnings()`; `mods/src/patches/parts/sync.cc` — tapped in `process_player_inventories()`, `process_entity_slots()`, `process_entity_slots_data()`

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
