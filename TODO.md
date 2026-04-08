﻿﻿# ??? STFC Community Mod - TODO List

## ? Completed

- Feature 1: Gamestate JSON Export (full + differential)
- Feature 1b: GitHub Gist sync (native, via cpr)
- Feature 2: Battle log export via CSV watcher
- Feature 4: Drydock assignments + peace shield in gamestate JSON
- Fix: export_request_mutex held across blocking I/O (log stall bug)
- Fix: all file paths now anchored to prime.exe directory (not CWD)

---

## ?? Active Bugs (fix before next release)

### BUG-1: Full export fires too frequently, constantly wiping delta file � FIXED
**Was:** `capture_player_data/buildings/ships` called `request_full_export()` /
`request_immediate_export()` unconditionally on every call. Game sends repeated
identical payloads at login, flooding full exports that wiped the delta file.
**Fix:** All three capture functions now compare incoming values against cached
state and only request an export when something actually changed.

### BUG-2: Peace shield detection incorrect � FULLY FIXED
**What's fixed:**
- Shield expiry read from `StarbaseDetailedScan` (EntityGroup type=57)
- False SHIELD ALERT at startup suppressed until first scan received
- `active`, `expires_at`, `expires_epoch`, `seconds_remaining` all correct
- Token count read from `cached_resources` using known resource IDs � verified
  534 tokens (1h�3 + 4h�177 + 8h�166 + 12h�117 + 1d�66 + 3d�4 + 30d�1)
- Peace shield changes tracked in differential export

**Trigger documented:** tap your station in the **system view** of your home
system. NOT the galaxy view, NOT interior/exterior button, NOT system button.
Documented in INSTALL.md and GIST_SETUP_COMPLETE.md.

**Still open (follow-up):**
- [ ] Golden peace shield (Scopely-applied after maintenance) � separate UI
      and countdown; not yet identified in proto data
- [ ] Auto-10min shield (triggered on first attack when unshielded) � verify
      it shows correctly via the same `StarbaseDetailedScan` path

### BUG-3: Fleet/hangar ships not populating drydock assignments � FIXED
**Was:** Code looked for `SLOTTYPE_FLEETPRESET` (type=7) in `EntitySlots`/
`EntitySlotsData` proto messages. That slot type is never sent.
**Fix:** Parse the `fleets` key in the JSON blob (EntityGroup type=42).
Each entry has a raw server-side `drydock_id` and `ship_ids`. Sort by
`drydock_id` ascending, re-index 1-5, export layer maps 1=A ... 5=E.
**Arrives automatically at login � no navigation required.**

### BUG-4: Alliance data overwritten with wrong alliance on successive exports
**Symptom:** Player is in alliance `GROW`, but successive exports show
different/incorrect alliance names, as if other players' alliance data is
overwriting the cached value.
**Suspected cause:** `AllianceProfiles` (EntityGroup type 71) is a bulk
response containing profiles for many alliances (the player's own plus
others seen nearby or in territory). The capture loop likely lacks a filter
to pick only the alliance the player is a member of, so whichever entry
arrives last in the repeated payload wins.
**Investigation needed:**
- [ ] Read `process_alliance_profiles` in `sync.cc` -- confirm whether
      the player's own alliance is identified correctly vs all others
- [ ] Check what field in `AllianceProfile` proto identifies membership
      (likely `memberUserIds` contains the player's own
      `Config::Get().game_state_player_id`)
- [ ] Fix capture to only accept the alliance the player is a member of,
      ignoring all other profiles in the response
- [ ] Verify fix: export should consistently show `GROW` across all
      successive exports after login

---

## ?? Pending Work

### Player profile data population — DONE
**Trigger confirmed:** UserProfiles (EntityGroup type 0) arrives automatically at login
but requires player_id to be set in [gamestate_export] for the mod to identify which
profile entry belongs to the current player.
**Fixes applied:**
- [x] Peace shield trigger identified: tap station on system view
- [x] Documented in INSTALL.md and GIST_SETUP_COMPLETE.md
- [x] Drydock assignments arrive automatically at login (no navigation needed)
- [x] player_id not set: mod now logs all seen userids with player names so the user
      can identify their own and copy it into the config
- [x] INSTALL.md and GIST_SETUP_COMPLETE.md updated with player_id setup instructions
      and how to find the userid from the log
### Feature: Syndicate level data � DONE
Syndicate level and XP read from `cached_resources` at export time:
- `Resource_Loyalty_Tier_HiddenToken` (id `1141922149`) = syndicate level
- `Resource_Loyalty_Points` (id `3374607211`) = syndicate XP
Both arrive automatically at login. Exported as `player.syndicate_level`
and `player.syndicate_xp` in the gamestate JSON.

### Feature 2: Battle log � outcome/ship empty on some entries � FIXED
**Was:** `outcome`, `ship`, and `location` resolved by matching `Player Name`
in the CSV against `cached_player_data.name`. If the name hadn't arrived yet
when the CSV was processed, those fields were left empty.
**Fix:** When name is unknown at import time, the entry ID is added to
`pending_battlelog_resolution`. When `capture_player_data` fires with a
non-empty name, `resolve_pending_battlelog_outcomes` re-scans the battlelog
file and back-fills the missing fields, then re-syncs to Gist.

### Feature: Missions � completed and in progress � DONE
- `missions_active` array: each entry has `instance_id` and `mission_id`;
  29 active missions captured at login
- `missions_completed` array: flat list of completed mission IDs;
  1433 completed missions captured at login
- Both arrive automatically at login � no navigation required
- Note: no name enrichment yet (missions not in `stfc_id_mappings.json`)
- Fix: `HandleEntityGroup` gate updated to also trigger on `export_gamestate`
  (previously only fired when `sync_options.missions` was enabled)

### Feature: Ship upgrade resource planning — PARTIALLY DONE
**What was implemented:**
- `BaseShipTierSpecs` (type 49) and `ShipTierSpecs` (type 50) hooked at login
- Each ship entry in `ships.json` now includes:
  - `tier_max_level`: max ship level achievable at the current tier
  - `tier_up_duration_secs`: seconds to complete a tier-up build for this hull
**Note:** Raw tier-up resource costs are NOT transmitted to the client —
they are computed server-side. No cost table is sent.
**TODO (still open):**
- [ ] `ShipTierSpec.tierStatModifiers` has cargo stats per tier (66=cargo_capacity,
      67=cargo_protection); could be exported if useful
### Feature: Faction store tokens and armada credits — DONE
**What was found:** `Resource_FactionToken_*` (store currencies) and
`Resource_Faction_SArmada_Credit_*` / `Resource_Faction_SArmadaDir_*`
(armada directives) were already in `cached_resources` at login.
No `Resource_FactionFavor_*` pattern exists — 'favour' in STFC is
represented by the `FactionToken` resources.
**Implemented:** `faction_store_tokens` and `armada_credits` sections added
to `faction.json`. Only non-zero amounts included. 12 token types and
6 armada credit types exported. Tests: 55/55.

### Feature: Faction favors (per-faction store bonuses) — DONE

**What was implemented:**
- `ShipBonusBuffSpecs` (EntityGroup type 51) hooked — exports `buffs.json`
  with full buff catalog: 1313 entries with modifier type, operation, per-level
  values, and faction affiliation where applicable.
- `FactionSpecs` (type 8) hooked — provides human-readable faction names and
  builds a `researchTreeId -> faction_name` map from `FactionSpec.researchTreeIds`.
- `ResearchSpecs` (type 66) newly hooked — maps each `ResearchProjectSpec` to
  its `researchTreeId`, giving a `researchId -> treeId` proxy (246 of 2361
  projects matched to a faction tree, all arriving at login).
- `ConsumableSpecs` (type 114) hooked — RESEARCH_UNLOCK entries with name pattern
  `Consumable_{prefix}_{favor}_{tier}` parsed into per-faction favor catalog.
- `faction_favors` in `faction.json` now fully populated with human-readable
  faction names, per-favor `tier` (player's current level, 0=not purchased),
  and `max_tier`.
- Full `modifier_code_name()` mapping with 60+ named modifier types.

**Live results (2026-04-07):**
- `Faction Bajoran` — 25 favors, 12 purchased
- `Faction ExBorg` — 41 favors, 3 purchased
- `Faction Federation` / `Klingon` / `Romulan` — 9 each
- `Faction Section31` — 22, `Faction Temporal` — 10, `Faction Terran` — 12
- `Faction Khan` — 30 (FKR prefix resolved via researchTreeIds)
- `LoopMuseum` / `Loyalty` / `Unlock` — internal/fake factions, no
  `researchTreeIds` in FactionSpec, kept as prefix fallback (expected)

Tests: 78/78.

### Feature: Research, build, scrap and repair queues
**Goal:** Export all active jobs and unlocked queue slot counts so AI assistants
can plan upgrade sequences and know what capacity the player has available.

**Queue types and their behaviour:**
| Queue type | Default slots | Purchasable extras | Notes |
|---|---|---|---|
| Research | 1 | Yes (player has 3) | Grouped in station queue UI |
| Station build | 1 | Yes (player has 3) | Grouped in station queue UI |
| Ship scrap | 1 | No | Grouped in station queue UI |
| Ship repair | 1 | Yes (max unknown) | One active repair per drydock slot; buying N extras allows N+1 simultaneous repairs |

**Data sources (all arrive via `Jobs`, EntityGroup type 56, only when jobs are active):**
- `JOBTYPE_RESEARCH = 3` → `ResearchParams { projectId, level }` + `Job.startTime`, `duration`, `reductionInSeconds`
- `JOBTYPE_STARBASECONSTRUCTION = 4` → `StarbaseConstructionParams { moduleId, level }` + same timing fields
- `JOBTYPE_SHIPSCRAP = 12` → `ScrapyardParams { hullId, shipId, level, componentIds[] }` + timing fields
- `JOBTYPE_REPAIRFLEET = 5` → `RepairFleetParams { fleetId }` + timing fields

**Queue slot count** arrives via `EntitySlots`/`EntitySlotsData` (types 117/121):
- Worker slots with `SlotSpec.workerSlotSpecParams.jobType == 3` = research queue slots
- Worker slots with `SlotSpec.workerSlotSpecParams.jobType == 4` = build queue slots
- Worker slots with `SlotSpec.workerSlotSpecParams.jobType == 12` = scrap queue slots (always 1)
- Worker slots with `SlotSpec.workerSlotSpecParams.jobType == 5` = repair queue slots
- Empty slots (no `jobId`) = available; occupied slots (has `jobId`) = in use
- Total slot count per type = number unlocked

**TODO:**
- [ ] Hook `Jobs` (type 56) for all four job types; for each active job export:
      `type`, `start_time`, `duration_secs`, `reduction_secs`, `finish_time` (computed UTC ISO-8601),
      plus type-specific fields (`projectId`/`moduleId`/`shipId`/`fleetId`, `level`)
- [ ] Count worker slots per job type from `EntitySlots`/`EntitySlotsData` (already parsed) —
      export `total` and `active` per queue type
- [ ] Enrich `projectId` with research name via `stfc_id_mappings.json`
- [ ] Enrich `moduleId` (building id) with building name via `stfc_id_mappings.json`
- [ ] Enrich `hullId` (scrap) and `fleetId` (repair) with ship name via `stfc_id_mappings.json`
- [ ] Export as a `queues` section in `player.json` with sub-keys `research`, `build`, `scrap`, `repair`
- [ ] Note: `Jobs` only fires when at least one job is active; if all queues are
      idle the section will be absent/empty — document this in the export schema

### Feature: Daily goals / tasks and Event data — AUDITED, NOT FEASIBLE VIA CURRENT INTERCEPT

**Audit findings (2026-04-07):**

Every EntityGroup type and every JSON blob key arriving at login was fully enumerated.
All unhandled EG types in the range 124-169 were decoded at runtime. Findings per type:

| EG type | Name | Content | Daily goals? |
|---|---|---|---|
| 124 | MarauderInfo | Static marauder hull/level ranges per galaxy node | No |
| 137 | ChallengeLadderSpecs | 12 PvP arena ladder specs with milestone rewards | No |
| 146 | ChallengeConfig | Single field: minimumLevelRequired | No |
| 148 | HazardSpecs | Hostile battle hazard type definitions | No |
| 150 | LoyaltySpecs | Syndicate loyalty track tiers (already exported) | No |
| 152 | WaveDefenseSyncData | Live wave defense battle state | No |
| 159 | AllianceLoyaltyStaticData | Alliance loyalty static spec data | No |
| 161 | LoyaltyTierRewards | Reward items per Syndicate loyalty tier | No |
| 162 | GameActivityRanksData | PvP arena rank definitions | No |
| 163 | GameActivity | Live arena/Surge match state | No |
| 164 | GameActivitySpecs | 3 entries: Activity_Arena_Default, Activity_Arena_Public, Activity_Surge_Default | No |
| 165 | GameActivityParticipantSpecs | Arena/Surge participant group definitions | No |
| 167 | GameActivityScheduleSpec | Arena scheduled days (Fri/Sat only) | No |

JSON blob keys checked: marauder_quick_scan_data (only arrives when in-system, not at login),
hazard_result_headers (recent hostile battle results, not goals), player_container / outpost_list
(in-system only). No daily_goals, event_goals, challenge, or similar key was ever observed.

**Verdict:** Daily goals, weekly events, and limited-time events are NOT exposed via the
HandleEntityGroup or JSON blob intercept paths at any point. The game's event/challenge UI
is driven by the Scopely platform layer (a separate HTTP API), not the PrimeServer proto stream.
This feature would require a separate intercept of the platform HTTP layer — significant new
work with no clear proto schema to decode against. Not worth pursuing given what is already exported.
### Feature: Alliance info and territory — DONE
**What was implemented:**
- Alliance name, tag, level, member count, power exported as structured `player.alliance`\r
  object in `player.json` (from `AllianceProfiles` type 71, arrives at login)
- `territory.json` — alliance held zones from `TerritoryAllianceSlots` (type 105)
  cross-referenced with `TerritoryStaticData` (type 96), both arrive at login
  Each held zone includes: `territory_id`, `tier`, `state` (owned/takeover),
  `takeover_windows` (weekday, start_hour_utc, duration_mins)
  Root fields: `total_slots`, `used_slots`\r
**Remaining non-starters:**
- Territory owner map (which alliance owns each of the 55 zones): `TerritoryAllOwners`\r
  (type 97) does NOT arrive at login — requires navigating to Territory screen
- Alliance diplomacy (war/peace/NAP): no bulk-get proto exists — only a push
  notification (`SetAllianceDiplomacy` type 89) when diplomacy *changes*. Not feasible.
### Feature: Refinery and faction store inventory — AUDITED, NOT FEASIBLE

**Audit findings (2026-04-07):**

#### Refinery
The refinery building (station building id=31) is already exported via uildings in
player.json at its current level. The refinery queue is driven by Jobs (EntityGroup
type 56, JOBTYPE_REFINEMENT=6), but Jobs only arrives when there are **active** jobs —
it does not fire at login when the refinery is idle. The EntitySlots/EntitySlotsData
worker slots at login carry JOBTYPE_AWAYASSIGNMENT=13 workers and empty slots; no
JOBTYPE_REFINEMENT slots appear at login. There is no static proto message describing
available refinement recipes — RefinementParams only carries {refinementId, amount}
inside a running Job, and no RefinementSpecs EntityGroup type exists. The refinery
is idle at login; no data is available without the player actively queuing a refinery job.

**What IS available (already exported):**
- player.json → uildings array includes the Refinery at its current level
- 
esources.json → all raw material quantities are already exported (player can
  cross-reference against known refinery recipes offline)

**What is NOT available (confirmed by exhaustive live probe 2026-04-07):**
- Refinery queue: Jobs type 56 only arrives when a job is active; nothing at login or refinery-open
- Refinery recipes: no static proto; client uses game data files
- Opening the refinery screen (Shift-F) was fully probed: every EntityGroup type and every
  Json blob key was logged before and after navigation. Zero new EG types arrive.
  Complete set of Json keys ever seen: alliance, alliance_container, alliance_contributions,
  alliance_diplomacy_relationships, alliance_job_help_info, alliance_member_activity,
  alliance_members, alliance_notifications, alliances_info, battle_result_headers,
  current_instance, defenses, deployed_fleets, docking_points, faction_standing, fleets,
  fulfilled_connection_requirements, gameworld_id, hazard_result_headers, instances,
  marauder_quick_scan_data, mining_slots, mission_mapping, my_deployed_fleets,
  my_shield_state, my_skill_data, officer_level_rewards, officer_synergy_factors,
  outpost_list, parent_system, player_container, resource_harvesters, resource_producers,
  resources, ships, starbase, starbase_modules, state, static_update, user_history,
  visited_systems. None contain refinery queue or faction store data.

#### Faction store
No FactionStoreResponse EntityGroup type exists in the proto (confirmed: all 207+
type IDs audited). The store is delivered through the Scopely platform layer
(Digit.Platform.Models.EntityGroup.TYPE_STOREOFFERS=2 or TYPE_JSON=4), but these
types overlap with PrimeServer EntityGroup type numbers and go through the same
HandleEntityGroup switch — Platform types (2, 4, 20, 21) map to unrelated PrimeServer
cases and are silently ignored. The game server URL (cdn-live-us1-web.startrek.digitgaming.com)
returns 404 for all guessed REST paths (/faction/store/get, /faction/store/state, etc.)
and /refinery/get. The faction store UI is loaded via the Platform layer on demand, not
pushed at login.

**What IS available (already exported):**
- action.json → action_store_tokens (amounts of each faction token the player holds)
- action.json → action_favors (per-faction store tier unlock levels, all tiers)
- action.json → action_reputation (current reputation per faction)
These together let an AI determine what tier of favor the player is at and how many
tokens they have — just not the live item list or costs from the store itself.

**Verdict:** Both sub-features require either (a) a separate IL2CPP intercept on the
Platform model layer (significant new work, no clear benefit vs. offline data), or (b)
static game-data files that enumerate recipes/store items (out of scope for runtime export).
Neither is worth pursuing given what's already exported covers the underlying resource state.
### Reorganisation: Consolidate all mod output into a single folder � DONE
**Decision:** `community_patch_settings.toml` and `community_patch_runtime.vars`
stay at game root (alongside `prime.exe`) to keep the diff with upstream minimal
and make merging easier.
**Final structure:**
```
<game root>/
  prime.exe
  community_patch_settings.toml   ? stays here (upstream compat)
  community_patch_runtime.vars    ? stays here (upstream compat)
  community_patch/
    community_patch.log
    game_data_maps/                ? moved from game root ?
      stfc_id_mappings.json
    game_state_exports/            ? moved from community_patch/ root ?
      player.json, ships.json, resources.json, research.json,
      officers.json, missions.json, faction.json, battlelog.json,
      manifest.json
```

---

## ??? Stale / Superseded items (kept for reference)

- Ships ID mappings � 113 ships now in stfc_id_mappings.json ?
- Officer traits mappings � 84 traits in stfc_id_mappings.json ?
- Faction reputation export � implemented via Resource_FactionPoint_* ?
- Blueprint parts export � implemented via Resource_*_Parts_* ?
- Feature 3: Discord TC webhooks � moved to stfc-discord-tc-notifications repo


**Next Immediate Steps:**
1. Extract ships data from Spock's Club
2. Add ships to ID mappings
3. Enhance ship export with full data
4. Add faction reputation and blueprints
5. Test thoroughly
6. Open PR to upstream

---

**Great work so far! The foundation is solid and working beautifully. The remaining work is mostly data collection and hooking into existing game data structures.** ??
