# STFC Community Mod - TODO

## Open Bugs

### BUG-7: is_damaged bleeds between ships in the same fleet - FIXED
In process_json, `fs.is_damaged` was not reset between ships in a fleet's ship_ids
loop — damage would bleed from ship A to ship B if B had no ship_dmg key.
Fixed by rewriting the check as a single expression that always evaluates to true or
false per ship: `fs.is_damaged = contains(ship_dmg) && contains(sid) && value > 0`.

---

## Investigate / Unknown

### INV-1: Peace shield - special shield types
Peace shield expiry and tokens are fully working for standard player-applied shields.
Two special cases have not been observed in proto data yet:
- Golden peace shield (Scopely-applied after maintenance) - unknown if it appears in
  my_shield_state or StarbaseDetailedScan, or uses a separate field/type entirely
- Auto-10min shield (triggered on first attack while unshielded) - unknown if it
  appears via the same my_shield_state path or a different one
Both require specific in-game conditions to observe. When either occurs, check
community_patch.log for "my_shield_state:" and "StarbaseDetailedScan:" lines.

### INV-2: Ship status enrichment - RESOLVED
The `is_mining` boolean field in `my_deployed_fleets` cleanly distinguishes
mining from stationary. Confirmed with Ship A (armada queued, state=0,
is_mining=false) and Ships B-E (mining, state=5, is_mining=true).

Implemented:
- `is_mining` captured into `DrydockEntry` and exported on both drydock
  entries (player.json) and ship entries (ships.json)
- `status` label overridden to `"mining"` when `is_mining=true`, replacing
  the ambiguous `"stationary"` label for those ships

Other open questions from the original investigation (warp destination,
node_id meaning) remain unresolved but are low priority.

---

## TODO

### TODO-1: Battle log CSV watcher - add enable/disable config - DONE
battle_log = false in [sync.game_state]. Documented in INSTALL.md and
example_community_patch_settings.toml.

### TODO-2: Ship status in drydock export - DONE
Each drydock entry now includes: status, system_id (omitted if 0), is_damaged, is_mining.
Status sourced from my_deployed_fleets Json blob — arrives at login AND is re-pushed
mid-session whenever fleet state changes (warp, recall, mining start/stop, etc.).
Mid-session updates handled by update_drydock_status() which diffs and triggers export.
DeployedFleetState enum mapped to player-facing labels:
  0=idle, 1=moving, 2=warping, 3=battling, 4=recalling, 5=stationary, 6=entering_combat
is_mining=true overrides label to "mining". Node-exhausted ships still show mining
until recalled (game reports is_mining=true until the ship moves — known limitation).
Status also annotated on each ship entry in ships.json.

### TODO-3: Research/build/scrap/repair queue export - DONE
Jobs (EntityGroup type 56) only arrives when at least one job is active.
Job types handled: RESEARCH=3, STARBASECONSTRUCTION=4, SHIPSCRAP=12, REPAIRFLEET=5.
Exported as queues section in player.json with sub-keys research, build, scrap, repair.
Each entry: start_time, duration_secs, reduction_secs, finish_time (UTC ISO-8601),
seconds_remaining, plus type-specific: project_name/module_name/ship_name + level.
IDs enriched with names via id_mappings. Section absent when all queues are idle.
Repair jobs also update drydock entry repair_active/repair_finish_epoch/repair_progress.
Not implemented: worker slot counts per job type (deferred, needs further investigation).

### TODO-4: Ship tier cargo stats - DONE
ShipTierSpec.tierStatModifiers has cargo stats per tier (66=cargo_capacity,
67=cargo_protection). BaseShipTierSpecs (type 49) and ShipTierSpecs (type 50) hooked.
cargo_capacity and cargo_protection exported per ship in ships.json, keyed by hull_id+tier.

### TODO-5: Artifact data export - INVESTIGATED, NOT FEASIBLE
Shard counts (Resource_Artifact_Pieces_N) are already exported in resources.json under
the "Other" group. Gacha tokens, hall upgrade tokens, and dust are also already there.

No dedicated artifact entity group type, InventoryItemType, or SlotType exists in the
proto. The game does not push artifact names, levels, or equipped-per-ship state through
any intercepted channel.

ResourceAutoConvertSpecs (type 203) + 'resource_auto_convert' blob fire on-demand when
navigating to the artifact hall — these contain shard overflow conversion specs (shard →
dust), not names or level data.

Artifact buffs, when active, flow into GlobalActiveBuffs (already in buffs.json) but are
not tagged as artifact-sourced.

Nothing new to export. Shard counts are readable from resources.json by filtering on the
Resource_Artifact_Pieces_N name pattern.

### TODO-6: Home system ID, station duration, relocation tokens in player export - DONE
All from StarbaseInfo in the "starbase" Json blob key (arrives at login).
Exported in player.json under station:
  home_system_id       - StarbaseInfo.location.system
  last_relocation_at   - ISO-8601 UTC from StarbaseInfo.lastRelocation
  days_in_system       - derived: (now - last_relocation_at) / 86400, 1 decimal place
  relocation_tokens    - Resource_RelocationToken (id 1638723607) from cached_resources
Peace shield also updated from inline peace_shield field in the same blob,
providing an earlier/more reliable source than my_shield_state.

### TODO-7: Alliance starbase info - DONE
AllianceStarbaseConfig (EntityGroup type 125) fires at login and contains
originsystemid — the system the alliance starbase is located in.
Exported as player.alliance.starbase_system_id in player.json.
Note: alliance starbase "state" (under attack, etc.) is not available through
any intercepted channel — StarbaseDetailedScan only fires for the player's own
station and has no system location field.

### TODO-8: example_community_patch_settings.toml - missing show_settings hotkey - DONE
show_settings = "SHIFT-S" added between show_scrapyard and show_ships.

---


Items marked (trigger: ...) require the user to navigate to a specific screen.

| Data | File | Source | Trigger |
|---|---|---|---|
| Player name, power, server | player.json | UserProfiles type 0 | auto |
| Alliance name/tag/level | player.json | AllianceProfiles type 71 + cache | auto (from cache) |
| Ops level | player.json | OPERATIONS building in buildings | auto |
| Syndicate level/XP | player.json | Resource_Loyalty_* IDs in resources | auto |
| Peace shield expiry | player.json | starbase + my_shield_state Json blob keys | auto |
| Peace shield tokens | player.json | cached_resources known IDs | auto |
| Station home system | player.json | Json blob "starbase" key (StarbaseInfo.location) | auto |
| Station last relocation | player.json | Json blob "starbase" key (StarbaseInfo.lastRelocation) | auto |
| Station days in system | player.json | derived from last_relocation_at | auto |
| Relocation tokens | player.json | Resource_RelocationToken in resources | auto |
| Buildings | player.json | Json blob "starbase_modules" | auto |
| Drydock assignments + ship status | player.json | Json blob "fleets" + "my_deployed_fleets" (is_mining) | auto |
| Ships in hangar | ships.json | Json blob "ships" key | auto |
| Blueprint parts | ships.json | cached_resources _Parts_ pattern | auto |
| Ship unlock BPs | ships.json | PlayerInventories type + BlueprintSpecs type 21 | auto |
| Resources | resources.json | Json blob "resources" key | auto |
| Research levels | research.json | ResearchTreesState type | auto |
| Officers | officers.json | Officers type | auto |
| Active missions | missions.json | ActiveMissions type | auto |
| Completed missions | missions.json | CompletedMissions type | auto |
| Faction reputation | faction.json | Resource_FactionPoint_* in resources | auto |
| Faction store tokens | faction.json | Resource_FactionToken_* in resources | auto |
| Armada credits | faction.json | Resource_Faction_SArmada_* in resources | auto |
| Faction favors | faction.json | ConsumableSpecs type 114 + ResearchSpecs 66 | auto |
| Active buffs | buffs.json | GlobalActiveBuffs type 69 | auto |
| Buff catalog | buffs.json | ShipBonusBuffSpecs type 51 | auto |
| Territory slots | territory.json | TerritoryAllianceSlots type 105 | auto |
| Territory specs | territory.json | TerritoryStaticData type 96 | auto |
| Battle log | battlelog.json | CSV file watcher | auto (file write) |

Not available / not feasible:
- Daily goals / events: driven by Scopely platform layer, not PrimeServer proto
- Refinery queue: Jobs type 56 only fires when a job is active, not at login
- Refinery recipes: no static proto exists
- Faction store item list: delivered via platform layer, not proto
- Territory owner map: TerritoryAllOwners (type 97) only arrives on Territory screen nav
- Alliance diplomacy: only pushed on change (SetAllianceDiplomacy type 89), no bulk-get

---

## Audited - Not Feasible

### Daily goals / events
Every EG type at login enumerated. No daily_goals, event_goals, or challenge key
observed in any Json blob. Event UI uses Scopely platform layer (separate HTTP API).
Not worth pursuing.

### Refinery and faction store
Refinery queue: Jobs type 56 only fires when active. No RefinementSpecs EG type.
Faction store: no FactionStoreResponse EG type. Delivered via Platform layer.
Already exported: faction tokens, armada credits, reputation, faction favors (tier levels).

---

## Fixed (summary)

- BUG-1: Full export flooding - fixed by change-detection in all capture functions
- BUG-2: Peace shield detection - fixed (partial, see open items above)
- BUG-3: Drydock assignments empty - fixed by parsing "fleets" Json blob key
- BUG-4: Alliance name overwritten with wrong alliance - fixed by scoping enrichment
         to configured player_id only, and checking cache immediately on UserProfiles
- BUG-5: Ship unlock blueprint counts missing - fixed by hooking BlueprintSpecs (type 21)
         and routing INVENTORYITEMTYPE_INVENTORYBLUEPRINT items to game state export
- BUG-6: Duplicate log entries and double-processing - fixed by FNV-1a content hash
         dedup in HandleEntityGroup; ProcessResultInternal and ParseBinaryObjectsHelper
         both fire for the same ServiceResponse, causing every entity group to be
         processed twice (3x for types also hooked via DataContainer_ParseBinaryObject)
- Config: export_request_mutex held across blocking I/O - fixed
- Config: all file paths anchored to prime.exe directory
- Config: TOML structure reorganised to [sync.game_state.*] tree
- Config: ResourceAlertsConfig -> ShieldAlertsConfig, field renamed shield_alerts
- Config: GistSyncConfig -> GitHubSyncConfig, field renamed game_state_github
- Config: export_gamestate* fields -> game_state_* fields
