# AI Assistant Context: SpotTheSpaceCat's STFC Game State

**Type:** Reference  
**Audience:** AI assistants (Claude, ChatGPT, etc.) helping the player with STFC planning.

---

## Who you are helping

**Player:** SpotTheSpaceCat  
**Ops level:** 41  
**Server:** 709  
**Alliance:** [GROW]  
**Game:** Star Trek Fleet Command (PC, Scopely) — a mobile-style 4X strategy game set in the Star Trek universe.

---

## How to access the game state

The player's live game state is exported automatically while they play via the STFC Community Mod. All data is stored in a GitHub Gist as separate JSON files.

**Start here — the manifest:**
```
https://gist.githubusercontent.com/DrCord/3d1bd2f24dc5ab3cc79beb9e8387b5e0/raw/stfc_manifest.json
```

The manifest lists every export file with its raw URL and a description of its contents. Fetch it first if you need to discover which file to load.

**For most questions — the summary:**
```
https://gist.githubusercontent.com/DrCord/3d1bd2f24dc5ab3cc79beb9e8387b5e0/raw/stfc_summary.json
```

The summary is a pre-computed compact digest (~13 KB). It covers the most common planning queries without loading large files.

---

## Which file to load for each query type

| Query | Best file |
|-------|-----------|
| General planning, resources, top ships, research overview | `stfc_summary.json` |
| Full ship roster (all ships, cargo stats, repair status) | `stfc_ships.json` |
| All research nodes in a specific tree | `stfc_research.json` |
| Officer abilities, ranks, shards | `stfc_officers.json` |
| All buildings and levels | `stfc_buildings.json` |
| Faction reputation and loyalty buffs | `stfc_faction.json` |
| Active job queues (build, research, scrap) | `stfc_player.json` |
| Active buffs and buff catalog | `stfc_buffs.json` |
| Territory takeover windows | `stfc_territory.json` |
| Ship blueprint progress | `stfc_resources.json` |

Use individual files only when the summary does not contain enough detail.

---

## Key data interpretation notes

### Resources
- `Resource_G{N}_{Material}_{Raw|R1|R2|R3}` — graded mining resources. N = grade (2–6). R1 = Common, R2 = Uncommon, R3 = Rare.
- At Ops 41, grade 3 (3★) resources are primary. Grade 2 is largely obsolete; grade 4 is increasingly relevant.
- `Resource_Parts_{Type}_{Grade}` — blueprint parts for ship construction (e.g. `Resource_Parts_Battleship_G3`).
- `Resource_FactionToken_{Faction}` — tokens spent in faction stores.

### Ships
- `repair_progress` is 0–100. It is the percentage of repair **completed**, not damage remaining. Current damage = `100 - repair_progress`.
- `cargo_protection` = protected cargo that survives a combat loss. `cargo_capacity` = total hold size.
- Ships are classified as **Survey** (mining) or combat (**Explorer**, **Battleship**, **Destroyer**) based on their blueprint name prefix `Blueprint_Hull_G{N}_{Class}_...`.
- `top5_mining` in the summary ranks survey ships by cargo_protection descending.
- `top5_combat` ranks combat ships by `tier × 100 + level` descending (per-ship power is not exported by the game sync).

### Officers
- Officers with a missing `name` field or `rank > 5` are phantom/placeholder entries — ignore them.
- `rank 0` = officer is not yet unlocked (owned shards, but not promoted). Hidden by default in the viewer.
- `shards` = **lifetime total acquired**. Spent shards are not subtracted. To calculate shards still needed to rank up, you need the rank-up cost for the next rank (not provided in the export).

### Buildings
- Personal station buildings have `id < 1000`.
- Alliance starbase buildings have `id >= 1001`. These reflect the alliance's investment, not SpotTheSpaceCat's personal station.

### Faction data
- `faction_reputation` in the summary is a flat `{faction: points}` object.
- `LoopMuseum` is a junk entry injected by the game and should be ignored if encountered in raw data.

### Research
- Trees prefixed `FC ` are **Fleet Commander** research — a separate progression system from the main research trees.
- The summary separates FC research into its own `fc_research` section.

### Ship blueprints
- `parts_needed` in `ship_blueprints_to_build` = the total number of blueprints required to unlock and build the ship.
- `amount` = blueprints currently held.
- Ships already in the hangar are excluded from `ship_blueprints_to_build`.

### Station
- `home_system_name` in `stfc_summary.json` has the system level stripped (e.g. "Freyda" not "Freyda (17)").
- `peace_shield.active` and `peace_shield.expires_at` (ISO 8601 UTC) indicate whether the station is protected from attack.

---

## STFC game context

A few concepts useful for interpreting the data:

| Concept | Meaning |
|---------|---------|
| Ops level | The player's station Operations building level — gates most progression |
| Tier | A ship's technology tier (1–9+); upgrading tier requires resources and a queue slot |
| Drydock | A deployment slot for a ship. SpotTheSpaceCat has multiple drydock slots (A–H) |
| Mining / Survey ships | Ships designed to mine resources; distinguished by high cargo_protection |
| Peace shield | A temporary protection from player attacks on the station |
| Faction reputation | Standing with game factions (Federation, Klingon, Romulan, etc.) that unlocks research and store access |
| Fleet Commander (FC) research | A separate research system unlocked at higher ops levels |
| Syndicate loyalty | Buff tiers earned through the Syndicate faction system; each tier unlocks passive combat and mining bonuses |

---

## Recommended workflow

1. **Fetch `stfc_summary.json`** for any general question.
2. If the summary lacks the detail needed (e.g. "show me all officers above rank 3"), **fetch the relevant specific file**.
3. If you need to know which files exist or their URLs, **fetch `stfc_manifest.json`** first.
4. Do not load large files (`stfc_officers.json` ~130 KB, `stfc_buffs.json` ~2 MB) unless the question specifically requires their content.
