# Reference: Game State Export Files

**Type:** Reference  
**Audience:** Developers and AI assistants querying the export data.

All export files are written to:
```
<game_dir>\community_patch\game_state_exports\
```
and synced to GitHub Gist when `[sync.game_state.github] enabled = true`.

---

## stfc_manifest.json

Index of all other export files. Contains raw Gist URLs so consumers can
discover files without hardcoding them.

```json
{
  "exported_at": "2026-04-09T22:00:00Z",
  "gist_base_url": "https://gist.githubusercontent.com/USERNAME/GISTID/raw",
  "files": [
    {
      "local_name": "player.json",
      "gist_filename": "stfc_player.json",
      "description": "...",
      "sections": ["player", "station", "drydocks", "queues"]
    },
    ...
  ]
}
```

---

## stfc_player.json

Player profile, station, drydocks, alliance, and active queues.

Key sections:

| Section | Fields |
|---------|--------|
| `player` | `name`, `ops_level`, `power`, `server` |
| `station` | `home_system_name`, `peace_shield` (`active`, `expires_at`), `relocation_tokens`, `days_in_system` |
| `drydocks` | Array of `{drydock, ship_name, tier, level, status, repair_active, repair_seconds_remaining}` |
| `alliance` | `name`, `tag`, `level`, `member_count`, `power` |
| `queues` | `build`, `research`, `scrap` — each an array of active jobs |

---

## stfc_ships.json

Full ship roster from the hangar.

Each ship object:

| Field | Description |
|-------|-------------|
| `hull_id` | Internal hull identifier |
| `name` | Human-readable ship name |
| `tier` | Current tier (1–9+) |
| `level` | Current level within tier |
| `tier_max_level` | Maximum level at current tier |
| `cargo_capacity` | Total cargo hold |
| `cargo_protection` | Protected cargo (survives combat loss) |
| `components` | Array of component IDs fitted |
| `drydock` | Which drydock slot (`A`–`H`) if assigned |
| `status` | `Docked`, `Mining`, `Away` |
| `repair_active` | `true` if repair in progress |
| `repair_seconds_remaining` | Seconds until repair completes |
| `repair_progress` | 0–100 — percent of repair completed (damage = `100 - repair_progress`) |

---

## stfc_resources.json

All player resources, blueprint parts, faction store tokens, and ship unlock blueprints.

Key sub-arrays:

| Array | Contents |
|-------|----------|
| `resources` | `{name, amount}` — all resources with non-zero amounts |
| `blueprints` | `{name, amount}` — blueprint part resource entries (`*_Parts_*`) |
| `ship_blueprints` | `{name, hull_id, amount, parts_needed}` — ship unlock blueprint counts |
| `faction_reputation` | `{faction, points}` — faction standing |
| `faction_store_tokens` | `{token, amount}` — faction store exchange tokens |

Resource name patterns:
- `Resource_Parsteel`, `Resource_Dilithium`, `Resource_Trianium` — base ores
- `Resource_G{N}_{Material}_{Raw|R1|R2|R3}` — graded mining resources (N = grade 2–6)
- `Resource_FactionToken_{Faction}` — faction store tokens
- `Resource_Parts_{Type}_{Grade}` — ship blueprint parts (e.g. `Resource_Parts_Battleship_G3`)

---

## stfc_research.json

All unlocked research nodes.

Each node: `{id, tree, name, level}`.

Research tree names containing the prefix `FC ` are Fleet Commander research.

---

## stfc_officers.json

Full officer roster.

Each officer:

| Field | Description |
|-------|-------------|
| `name` | Officer name (missing = phantom/placeholder, exclude from display) |
| `rank` | 0 = not yet unlocked; 1–5 = Ensign through Commander |
| `level` | Current level |
| `shards` | Lifetime shards acquired (spent shards are not removed) |
| `faction` | Officer faction |
| `rarity` | `Common`, `Uncommon`, `Rare`, `Epic` |
| `traits` | Array of `{id, name, description}` |

---

## stfc_buildings.json

Station buildings and levels.

Each building: `{id, name, level}`.

- Personal station buildings: `id < 1000`
- Alliance starbase buildings: `id >= 1001`

---

## stfc_faction.json

Faction standing, loyalty buffs, faction favors, and faction store tokens.

Key sections:

| Section | Contents |
|---------|----------|
| `faction_reputation` | `{faction, resource_id, points}` per faction |
| `syndicate_loyalty_buffs` | Active loyalty tier per syndicate faction |
| `faction_favors` | Research-based faction favor levels |
| `faction_store_tokens` | Exchange token amounts |

---

## stfc_buffs.json

Full active buff catalog.

Each entry: `{buff_id, modifier, modifier_code, operation, ranked_values, faction?}`.

`ranked_values` is an array where index = level - 1.

---

## stfc_territory.json

Territory control slots and takeover windows.

Each slot: `{system_name, system_level, takeover_start_utc_hour, takeover_end_utc_hour, holder?}`.

---

## stfc_missions.json

Mission progress data.

---

## stfc_summary.json

Pre-computed compact digest for token-efficient AI queries. Avoids loading
large files for common planning questions.

Top-level sections:

| Section | Contents |
|---------|----------|
| `player` | Full player object (same as `stfc_player.json`) |
| `station` | Station object with `home_system_name` stripped of level suffix |
| `drydocks` | Full drydocks array |
| `ships` | `{count, top5_mining, top5_combat}` — survey ships sorted by cargo_protection; combat ships sorted by tier×100+level |
| `resources` | Aggregated: `parsteel`, `dilithium`, `tritanium`, `latinum`, `mining_raw`, `mining_refined` (by grade+material+rarity), `faction_tokens`, `ship_parts` (nested by type then grade) |
| `faction_reputation` | Flat `{faction: points}` object, junk entries excluded |
| `buildings_key` | Key personal station buildings only (`id < 1000`) |
| `ship_blueprints_to_build` | Ships not yet in hangar with `amount >= 1` and grade ≥ 3 (non-placeholder) — includes `parts_needed` |
| `queues` | Active job queues |
| `research` | `{unlocked_count, by_tree, fc_research: {unlocked_count, by_tree}}` |

Mining resources in the summary are filtered to grades relevant to the player's
ops level: at ops level N, grades below `N/10 - 1` are excluded.
