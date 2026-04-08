# STFC Community Mod - TODO

## Open Bugs

### BUG-2 (partial): Peace shield edge cases
Fixed: expiry from StarbaseDetailedScan (type 57), tokens from cached_resources.
Trigger: tap station in system view (NOT galaxy view, NOT interior/exterior button).
Still open:
- [ ] Golden peace shield (Scopely-applied after maintenance) - not yet in proto data
- [ ] Auto-10min shield (triggered on first attack) - verify via StarbaseDetailedScan path
- [ ] Verify whether tapping the peace shield icon on the exterior station screen also
      triggers StarbaseDetailedScan - if so, document as a second valid trigger

---

## TODO

### TODO-1: Battle log CSV watcher - add enable/disable config
Add a toml config param under [sync.game_state] (or a new [battle_log] section) to
enable/disable the CSV watcher. Default: disabled. Document in INSTALL.md and
example_community_patch_settings.toml.

### TODO-2: Ship status in drydock export
Currently drydock assignments export only which ship is in which slot.
Investigate what status data is available per drydock slot. Known possible statuses:
repairing, defending station (home/idle), away and idle, fighting, mining,
warping between systems, moving in-system via impulse.
Data likely in the Json blob "deployed_fleets" or "fleets" keys - check at login
and after navigating to different views.
Data source trigger: unknown - needs investigation.

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

---

## Data Sources Reference

All items marked (auto) arrive at login with no navigation required.
Items marked (trigger: ...) require the user to navigate to a specific screen.

| Data | File | Source | Trigger |
|---|---|---|---|
| Player name, power, server | player.json | UserProfiles type 0 | auto |
| Alliance name/tag/level | player.json | AllianceProfiles type 71 + cache | auto (from cache) |
| Ops level | player.json | OPERATIONS building in buildings | auto |
| Syndicate level/XP | player.json | Resource_Loyalty_* IDs in resources | auto |
| Peace shield expiry | player.json | StarbaseDetailedScan type 57 | tap station in system view |
| Peace shield tokens | player.json | cached_resources known IDs | auto |
| Buildings | player.json | Json blob "starbase_modules" | auto |
| Drydock assignments | player.json | Json blob "fleets" key | auto |
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
