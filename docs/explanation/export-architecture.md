# Explanation: Game State Export Architecture

**Type:** Explanation  
**Audience:** Contributors and anyone wanting to understand why the export is designed the way it is.

---

## Why per-file exports instead of one big JSON

The early design explored a single monolithic `stfc_gamestate.json` and a differential export scheme. Both were abandoned.

**Single-file problems:**
- A full export is ~2.5 MB. GitHub Gist has a soft limit per file, and AI context windows have hard token limits. Loading the entire game state to answer "how many parsteel do I have?" wastes both.
- Gist's raw URL for a file changes on every edit. A single file means every field changes on every export, making the URL-based cache useless.

**Differential export problems:**
- Requires tracking previous state in memory, increasing DLL complexity and the risk of divergence bugs.
- An AI assistant that loads the delta still needs the full baseline to understand context. Two-file coordination is harder than one up-to-date file.

**Per-file approach:**
- Each file changes only when its category changes. Gist caches unchanged files.
- An AI assistant fetches only what it needs. Officers rarely change; resources change often.
- `stfc_summary.json` gives a single-fetch answer for the most common planning queries.
- The manifest (`stfc_manifest.json`) provides a stable discovery URL; individual file URLs may change but the manifest URL is fixed.

---

## Why stfc_summary.json exists

AI assistants are billed per token. Loading `stfc_resources.json` (~90 KB) to answer "what is my parsteel count?" consumes thousands of tokens unnecessarily.

`stfc_summary.json` (~13 KB) is pre-computed in C++ at export time and contains:
- Aggregated resource totals grouped by type (not raw per-resource entries)
- Top-5 mining and combat ships (not all 29 ships)
- Key building levels (not all 60+ buildings)
- Research counts by tree (not all 400+ nodes)
- Compact faction reputation
- Buildable ship blueprints

The tradeoff: `stfc_summary.json` is lossy by design. When an AI needs complete detail — all officers, full research tree, every resource — it should load the relevant specific file.

---

## How sync works

The DLL handles all sync internally. There is no separate Python script.

On export trigger (startup, interval, or data-change event):
1. `export_game_state()` calls `build_game_state_json()` which assembles all sections from in-memory caches (populated by IL2CPP hooks during normal gameplay).
2. Each section is written to a separate JSON file in `community_patch\game_state_exports\`.
3. `build_summary_json()` computes the compact digest from the full game state.
4. If Gist sync is enabled, `sync_all_to_gist()` PATCHes all changed files to the Gist in a single API call using the `cpr` HTTP library.

The DLL does not diff against previous state before uploading — it uploads all files on every export. GitHub Gist deduplicates unchanged content server-side.

---

## How ship names are resolved

The game's protobuf sync provides `hull_id` but not a human-readable ship name. Names are resolved in two ways:

1. **IL2CPP id_mappings** (`id_mappings::MappingCache`) — enriches ship entries with `name` from a pre-built lookup table derived from game data maps in `community_patch/game_data_maps/`.
2. **Blueprint name parsing** — ship blueprint names follow the pattern `Blueprint_Hull_G{N}_{Class}_{Faction}_{ShipName}`. The class (`Survey`, `Explorer`, `Battleship`, `Destroyer`) is extracted from this to classify ships as mining vs combat in the summary.

---

## Why the summary filters low-grade mining resources

At ops level 41, grade-2 mining resources are obsolete — their storage caps are so low the player is unlikely to accumulate meaningful amounts. Including them in the summary adds noise without value.

The filter formula: `min_grade = ops_level / 10 - 1` (integer division, clamped to ≥ 1). At ops 41 this gives grade 3, so G1/G2 resources are excluded from `mining_raw` and `mining_refined` in the summary. The full resources file is unaffected.

---

## Why alliance starbase buildings are excluded from buildings_key

The `buildings_key` section in the summary is intended as a quick reference to the player's personal station capability gates (shipyard level → max ships, operations level → ops level cap, etc.). Alliance starbase buildings (`id >= 1001`) reflect the alliance's collective investment, not the individual player's station, and are not useful for personal planning queries.
