# STFC Community Mod - TODO

## Open Bugs

None.

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

### INV-2: Ship status enrichment - needs live data collection
Current status values come from DeployedFleetState in my_deployed_fleets.
Open questions requiring live test sessions:
- stationary vs mining: state=5 covers both. Does my_deployed_fleets gain any new field
  when a ship starts mining? Does mining_slots arrive in the blob at that point?
  To test: start a ship mining, re-login, check log for new fields on that fleet entry.
- warp destination: proto DeployedFleet has warp_data and destination_node_id. Do these
  appear in my_deployed_fleets when warping? To test: send a ship on a long warp, login
  mid-warp, check my_deployed_fleets entry for warp_data and destination_node_id.
- node_id meaning: non-zero for all stationary ships - may identify mining node vs orbit
  if a node ID to resource type mapping exists in static data.

---

## TODO

### TODO-1: Battle log CSV watcher - add enable/disable config - DONE
battle_log = false in [sync.game_state]. Documented in INSTALL.md and
example_community_patch_settings.toml.

### TODO-2: Ship status in drydock export - DONE
Each drydock entry now includes: status, system_id (omitted if 0), is_damaged.
Status sourced from my_deployed_fleets Json blob key (arrives at login).
DeployedFleetState enum mapped to player-facing labels:
  0=idle, 1=moving, 2=warping, 3=battling, 4=recalling, 5=stationary, 6=entering_combat
Note: stationary covers mining AND idle-away - see INV-2 for open investigation.
Status also annotated on each ship entry in ships.json.

### TODO-3: Research/build/scrap/repair queue export
Jobs (EntityGroup type 56) only arrives when at least one job is active.
Job types: RESEARCH=3, STARBASECONSTRUCTION=4, SHIPSCRAP=12, REPAIRFLEET=5.
Queue slot counts from EntitySlots/EntitySlotsData (types 117/121) already parsed.
- [ ] Hook Jobs (type 56) for all four job types
- [ ] Export type, start_time, duration_secs, reduction_secs, finish_time (UTC ISO-8601)
      plus type-specific: projectId / moduleId / shipId / fleetId, level
- [ ] Count worker slots per job type: total and active
- [ ] Enrich IDs with names via stfc_id_mappings.json
- [ ] Export as queues section in player.json with sub-keys research, build, scrap, repair
- [ ] Document that the section is absent when all queues are idle

### TODO-4: Ship tier cargo stats
ShipTierSpec.tierStatModifiers has cargo stats per tier (66=cargo_capacity,
67=cargo_protection). BaseShipTierSpecs (type 49) and ShipTierSpecs (type 50) already
hooked. Decide whether to export cargo_capacity and cargo_protection per ship.

### TODO-5: Artifact data export
Export active artifacts with their buffs and shard counts toward unlocking/leveling.
Needs investigation - unknown EntityGroup type or Json blob key.
- Are active artifact buffs in the existing buffs.json flow or separate?
- Are shard counts in cached_resources or a separate inventory type?
Data source trigger: unknown - needs investigation.

### TODO-6: Home system ID in player export
StarbaseInfo.location is a NodeAddress with a system field (the player home system ID).
The "starbase" Json blob key (observed at login) contains StarbaseInfo inline.
- [ ] Add handler for "starbase" key in process_json
- [ ] Export as player.station.home_system_id
Data source: auto at login.

### TODO-7: Alliance starbase info
AllianceProfile (type 71) does not include starbase location or state.
The "alliance_container" Json blob key (observed at login) likely carries this.
StarbaseInfo.location.system = alliance starbase home system.
StarbaseInfo.state = starbase state (moving, anchored, etc).
- [ ] Log raw "alliance_container" to confirm structure
- [ ] Export alliance.starbase_system_id and alliance.starbase_state in player.json
Data source: likely auto at login via alliance_container blob key.

### TODO-8: example_community_patch_settings.toml - missing show_settings hotkey
show_settings = "SHIFT-S" present in community_patch_settings_old.toml but missing
from example_community_patch_settings.toml between show_scrapyard and show_ships.
- [ ] Add show_settings = "SHIFT-S" in [hotkeys] section

---


Items marked (trigger: ...) require the user to navigate to a specific screen.

| Data | File | Source | Trigger |
|---|---|---|---|
| Player name, power, server | player.json | UserProfiles type 0 | auto |
| Alliance name/tag/level | player.json | AllianceProfiles type 71 + cache | auto (from cache) |
| Ops level | player.json | OPERATIONS building in buildings | auto |
| Syndicate level/XP | player.json | Resource_Loyalty_* IDs in resources | auto |
| Peace shield expiry | player.json | my_shield_state Json blob key | auto |
| Peace shield tokens | player.json | cached_resources known IDs | auto |
| Buildings | player.json | Json blob "starbase_modules" | auto |
| Drydock assignments + ship status | player.json | Json blob "fleets" + "my_deployed_fleets" | auto |
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
- Config: export_request_mutex held across blocking I/O - fixed
- Config: all file paths anchored to prime.exe directory
- Config: TOML structure reorganised to [sync.game_state.*] tree
- Config: ResourceAlertsConfig -> ShieldAlertsConfig, field renamed shield_alerts
- Config: GistSyncConfig -> GitHubSyncConfig, field renamed game_state_github
- Config: export_gamestate* fields -> game_state_* fields
