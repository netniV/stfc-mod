# STFC Community Mod — Feature Development Specification

**Project:** `stfc-community-mod` (fork of `netniV/stfc-mod`)
**Repo:** `https://github.com/DrCord/stfc-community-mod`
**Active branch:** `feature/sync_game_state` (merge target: `main`)
**Language:** C++ (MSVC, C++17)
**Build system:** XMake — run `xmake` from repo root to build the DLL
**Game:** Star Trek Fleet Command (Windows PC client, Scopely)
**Player:** DrCord — Ops 41, Server 709, Alliance [GROW]

---

## How the Mod Works

The mod is a Windows proxy DLL (`version.dll`) placed in the STFC game directory:
```
C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game\version.dll
```

When the game launches it loads `version.dll`, which hooks into the game process using IL2CPP
function interception. The mod reads game protobuf data from network responses in real time
and exports structured JSON snapshots to disk (and optionally to GitHub Gist).

Configuration is via a TOML file in the same directory:
```
community_patch_settings.toml
```

### Key source files (all under `mods/src/`)

| File | Purpose |
|------|---------|
| `config.cc` | TOML config loading |
| `file.cc` | File I/O utilities |
| `patches/patches.cc` | Entry point wiring all patch modules |
| `patches/parts/sync.cc` | Data interception hooks — primary site for new EntityGroup handlers |
| `patches/parts/game_state_export.cc` | Game state capture, JSON assembly, file export, Gist sync |
| `patches/parts/game_state_export.h` | Public API for capture functions called from sync.cc |
| `patches/parts/id_mappings.cc` | Loads `stfc_id_mappings.json` for ID→name enrichment |
| `patches/parts/hotkeys.cc` | Keyboard shortcut handling |
| `prime/proto/*.proto` | Protobuf definitions for all game data structures |

### Data flow

```
Game network response
  → HandleEntityGroup() in sync.cc
    → process_*() helper (parses proto bytes)
      → game_state_export::capture_*() (stores in cached_* statics under game_data_mutex)
        → request_immediate_export() (signals export thread)
          → export_thread_func() → build_game_state_json() → write per-section files
            → sync_to_gist() (if enabled)
```

### ID/name lookup files (`community_patch/game_data_maps/`)

| File | Contents | Source |
|------|----------|--------|
| `stfc_id_mappings.json` | Officers, research, buildings, resources, ships, traits | Scraped/maintained manually |
| `stfc_systems.json` | 2452 system ID → name mappings | Scraped from stfc.space via `scripts/data_extraction/scrape_stfc_systems.py` |

---

## Feature 1: Game State JSON Export ✅ IMPLEMENTED

**Goal:** Export a structured JSON snapshot of the player's current game state to separate
per-section files on disk while the game is running. Enables external tools and AI assistants
to read current game data without manual interaction.

### Output files (`community_patch/game_state_exports/`)

| File | Description |
|------|-------------|
| `manifest.json` | Index of all export files with descriptions and optional Gist URLs |
| `player.json` | Player profile, station, drydocks, alliance, job queues |
| `buildings.json` | Station buildings with current level |
| `ships.json` | Ships in hangar with stats, cargo, blueprints |
| `resources.json` | All non-zero resource amounts |
| `research.json` | All unlocked research nodes with levels |
| `officers.json` | Officers with rank, level, shards, trait levels |
| `missions.json` | Active and completed missions |
| `faction.json` | Faction reputation, tokens, credits, favors, syndicate loyalty |
| `buffs.json` | Active buffs snapshot + full buff catalog |
| `territory.json` | Alliance territory holdings with names, tiers, takeover windows |
| `battlelog.json` | Battle history (last 500 battles) |

### player.json structure

```json
{
  "exported_at": "2026-04-08T19:12:28Z",
  "player": {
    "name": "DrCord",
    "ops_level": 41,
    "power": 19714080,
    "server": 709,
    "syndicate_level": 3,
    "syndicate_xp": 12000,
    "alliance": "GROW",
    "alliance_tag": "GROW",
    "alliance_id": 12345,
    "alliance_level": 10,
    "alliance_member_count": 48,
    "alliance_power": 5000000000,
    "alliance": {
      "name": "GROW", "tag": "GROW", "level": 10, "member_count": 48, "power": 5000000000,
      "starbase_system_id": 78790,
      "starbase_system_name": "Agrico (1)"
    }
  },
  "station": {
    "home_system_id": 1113049160,
    "home_system_name": "Zhang (19)",
    "last_relocation_at": "2026-04-06T16:55:00Z",
    "days_in_system": 2.4,
    "relocation_tokens": 165,
    "peace_shield": {
      "active": true,
      "expires_at": "2026-04-09T13:35:34Z",
      "expires_epoch": 1775741734,
      "seconds_remaining": 40829,
      "token_count": 533
    }
  },
  "drydocks": [
    {
      "drydock_id": 1, "letter": "A",
      "ship_id": 2650595741252535544,
      "status": "mining",
      "is_damaged": false,
      "is_mining": true,
      "system_id": 1324059029,
      "system_name": "Innlasn Alpha (20)",
      "is_at_home_station": false
    }
  ],
  "queues": {
    "research": [
      {
        "project_name": "Interceptor Weapons",
        "level": 5,
        "start_time": "2026-04-08T18:00:00Z",
        "duration_secs": 7200,
        "reduction_secs": 0,
        "finish_time": "2026-04-08T20:00:00Z",
        "seconds_remaining": 1234
      }
    ],
    "build": [],
    "scrap": []
  }
}
```

Notes:
- `drydocks[].is_at_home_station` = true when ship is idle (state=0, not mining) in the home system
- `drydocks[].system_name` comes from `stfc_systems.json` lookup on `system_id`
- `station.home_system_name` comes from the same lookup
- `queues` section is absent when all queues are idle
- Repair jobs are reflected on drydock entries (`repair_active`, `repair_finish_epoch`, `repair_progress`) rather than in `queues`
- Peace shield data sourced from `starbase` blob at login (primary) and `my_shield_state` blob on station navigation

### ships.json structure (per ship)

```json
{
  "id": 111, "hull_id": 222, "name": "Defiant",
  "tier": 4, "level": 20, "tier_max_level": 25,
  "tier_up_duration_secs": 86400,
  "cargo_capacity": 12000.0,
  "cargo_protection": 3000.0,
  "status": "idle",
  "drydock": "A", "drydock_id": 1,
  "components": [ { "id": 1, "level": 5 } ],
  "blueprint_parts": 47,
  "ship_blueprints": 2
}
```

### territory.json structure

```json
{
  "territory": {
    "total_slots": 5,
    "used_slots": 2,
    "held": [
      {
        "territory_id": 716012578,
        "name": "Innlasn",
        "state": "owned",
        "tier": 2,
        "node_ids": [1324059029, 987654321],
        "takeover_windows": [
          { "weekday": 3, "weekday_name": "Wed", "start_hour_utc": 20, "duration_mins": 60 }
        ]
      }
    ]
  }
}
```

Notes:
- `name` is derived automatically: find the first `node_id` whose system name (from `stfc_systems.json`) contains " Alpha"/" Beta"/" Gamma", then strip that suffix
- `node_ids` are the system IDs within the territory — useful for cross-referencing with drydock `system_id`

### Config (`[sync.game_state]` in `community_patch_settings.toml`)

```toml
[sync.game_state]
enabled         = true
export_dir      = ''          # empty = game directory / community_patch/game_state_exports/
player_id       = 'y806e96e...'
battle_log      = false       # enable CSV battle log watcher

[sync.game_state.github]
enabled              = false
gist_id              = 'your_gist_id_here'
token                = 'ghp_your_token_here'
username             = 'DrCord'
filename_manifest    = 'manifest.json'
filename_player      = 'player.json'
filename_buildings   = 'buildings.json'
filename_ships       = 'ships.json'
filename_resources   = 'resources.json'
filename_research    = 'research.json'
filename_officers    = 'officers.json'
filename_missions    = 'missions.json'
filename_faction     = 'faction.json'
filename_buffs       = 'buffs.json'
filename_territory   = 'territory.json'
filename_battlelog   = 'battlelog.json'
```

### Data sources

| Data | Source | EntityGroup type / blob key |
|------|--------|---------------------------|
| Player name, power, server | UserProfiles | type 0 |
| Alliance name/tag/level/power | AllianceProfiles + cache | type 71 |
| Alliance starbase system | AllianceStarbaseConfig | type 125 |
| Ops level | OPERATIONS building in buildings | — |
| Syndicate level/XP | Resource_Loyalty_* in resources | — |
| Peace shield expiry | `starbase` blob (primary), `my_shield_state` blob | Json blob keys |
| Peace shield tokens | cached_resources known IDs | — |
| Station home system | `starbase` blob → StarbaseInfo.location.system | Json blob key |
| Station last relocation | `starbase` blob → StarbaseInfo.lastRelocation | Json blob key |
| Relocation tokens | Resource_RelocationToken (id 1638723607) | — |
| Buildings | `starbase_modules` blob | Json blob key |
| Drydock assignments | `fleets` blob (EntitySlots/EntitySlotsData) | Json blob key |
| Ship status (state, system, mining) | `my_deployed_fleets` blob + mid-session updates | Json blob key |
| Ships in hangar | `ships` blob | Json blob key |
| Blueprint parts | cached_resources _Parts_ pattern | — |
| Ship unlock BPs | PlayerInventories + BlueprintSpecs | types 46 + 21 |
| Ship tier cargo stats | BaseShipTierSpecs + ShipTierSpecs | types 49 + 50 |
| Resources | `resources` blob | Json blob key |
| Research levels | ResearchTreesState | type (custom) |
| Officers | Officers | type (custom) |
| Active missions | ActiveMissions | type (custom) |
| Completed missions | CompletedMissions | type (custom) |
| Faction reputation | Resource_FactionPoint_* in resources | — |
| Faction store tokens | Resource_FactionToken_* in resources | — |
| Armada credits | Resource_Faction_SArmada_* in resources | — |
| Faction favors | ConsumableSpecs + ResearchSpecs | types 114 + 66 |
| Active buffs | GlobalActiveBuffs | type 69 |
| Buff catalog | ShipBonusBuffSpecs | type 51 |
| Territory specs (tier, windows, nodes) | TerritoryStaticData | type 96 |
| Territory alliance slots | TerritoryAllianceSlots | type 105 |
| Job queues (research/build/scrap/repair) | Jobs | type 56 |
| System names | `stfc_systems.json` (offline lookup) | — |

### Known limitations / not available

- Daily goals / events: driven by Scopely platform layer, not PrimeServer proto
- Refinery recipes: no static proto exists
- Faction store item list: delivered via platform layer, not proto
- Territory owner map: TerritoryAllOwners (type 97) only arrives on Territory screen nav
- Alliance diplomacy: only pushed on change, no bulk-get at login
- Territory system names: Unity localization strings — derived indirectly via `stfc_systems.json` node ID lookup
- Special peace shields (Scopely maintenance shield, auto-10min combat shield): observed in standard `my_shield_state` path but not yet confirmed for edge cases

---

## Feature 1b: Gist Sync ✅ IMPLEMENTED

After every successful export, each JSON file is PATCHed to the configured GitHub Gist via
the Gist API (`PATCH https://api.github.com/gists/{gist_id}`). The manifest lists the raw
Gist URLs for each file. Gist URLs are logged at startup.

Config is under `[sync.game_state.github]` — see Feature 1 above.

---

## Feature 2: Peace Shield Alerts ✅ IMPLEMENTED

Logs `spdlog::warn` alerts when:
- The peace shield is currently **down** (with token count)
- The peace shield will expire within a configured threshold (default: 4h, 2h, 1h)

```
SHIELD ALERT: Station peace shield is DOWN. Tokens in inventory: 2
SHIELD ALERT: Station peace shield expires in 1h 47m (threshold: 2h)
```

Config (`[sync.shield_alerts]`):
```toml
[sync.shield_alerts]
enabled                    = true
poll_interval_seconds      = 60
reminder_interval_minutes  = 30
shield_warn_hours          = "4,2,1"
```

---

## Feature 3: Alliance Discord Webhook — OUT OF SCOPE

Belongs in `stfc-discord-tc-notifications` repo, not here.

---

## Development Workflow

### Build and deploy

```bash
# From repo root — builds mods.lib AND links the final DLL
xmake

# Deploy
copy build\windows\x64\debug\stfc-community-mod.dll \
     "C:\Games\Star Trek Fleet Command\...\default\game\version.dll"

# Launch
Start-Process "C:\Games\Star Trek Fleet Command\...\default\game\prime.exe"

# Watch log
tail -f "C:\Games\Star Trek Fleet Command\...\default\game\community_patch.log"
```

> **Important:** `xmake build mods` only rebuilds `mods.lib`, not the final DLL.
> Always use bare `xmake` to ensure the link step runs.

### Adding a new EntityGroup handler

1. Add a `process_*()` function in `sync.cc`
2. Wire it into the `HandleEntityGroup()` switch with `submit_async()`
3. Add a `capture_*()` function to `game_state_export.h` / `.cc`
4. Call it from your `process_*()` function
5. Use the captured data in `build_game_state_json()` in `game_state_export.cc`

### Logging
```cpp
#include <spdlog/spdlog.h>
spdlog::info("GameState: loaded {} items", count);
spdlog::warn("GameState: shield is down, tokens={}", tokens);
spdlog::error("GameState: file write failed: {}", ec.message());
```

### JSON (nlohmann_json)
```cpp
#include <nlohmann/json.hpp>
using json = nlohmann::json;
json j;
j["player"]["ops_level"] = 41;
j["buildings"] = json::array();
j["buildings"].push_back({ {"id", 0}, {"name", "Operations"}, {"level", 41} });
std::string output = j.dump(2);
```

---

## PR Strategy

Developing on `feature/sync_game_state` in `DrCord/stfc-community-mod`.
When stable, open a PR to `netniV/stfc-mod:dev`. Coordinate in mod Discord (`discord.gg/PrpHgs7Vjs`).

---

## Dependencies

| Library | Version | Use |
|---------|---------|-----|
| `nlohmann_json` | v3.12.0 | JSON serialization |
| `spdlog` | v1.17.0 | Logging |
| `toml++` | v3.4.0 | Config file parsing |
| `libcurl` / `cpr` | 8.11.0 / 1.14.2 | HTTP (Gist sync) |
| `protobuf` | 32.1 | Game data deserialization |
| `abseil` | 20250512.1 | Utility (protobuf dependency) |

---

## Notes

- **GPL-3.0 license** — all contributions must be open source
- **Game ToS awareness** — read-only data export, no game modification
- **Debug builds** are fine for development; release builds for distribution
- **Never commit to `main` directly** — always branch and PR
