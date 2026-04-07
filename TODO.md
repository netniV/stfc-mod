# ??? STFC Community Mod - TODO List

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

### Feature: Faction favors (per-faction store bonuses) — NEEDS FURTHER INVESTIGATION
**What was implemented:**
- `ShipBonusBuffSpecs` (EntityGroup type 51) hooked — exports `buffs.json`
  with full buff catalog: 1313 entries with modifier type, operation, per-level
  values, and faction affiliation where applicable.
- `FactionSpecs` (type 8) hooked — provides human-readable faction names
  (Faction Federation, Faction Klingon, Faction Romulan etc.) for the catalog.
- `faction_favors` key added to `faction.json` (currently empty array).
- Full `modifier_code_name()` mapping with 60+ named modifier types.
Tests: 60/60.

**Still open — true Bajoran/Federation/etc. per-faction store favors:**
The `ShipBonusBuffSpecs` faction buffs (42 entries, ~5-6 per big faction) are
research/meta-progression bonuses, NOT the named per-item store bonuses shown
in the Bajoran Favors screenshot (Quantum Torpedoes, Bajor's Rage, etc.).
None of the player's 77 active `GlobalActiveBuffs` IDs appear in the
`ShipBonusBuffSpecs` catalog — the Bajoran Favors come from a different source.
**Investigation needed:**
- [ ] Identify which EntityGroup type carries the named faction-store favor specs.
      Candidates: `ActivatedAbilitySpecs` (type 128), `OfficerAbilityBuffSpecs`
      (type 17), or an unhanded type in the switch. Enable debug logging to
      enumerate all type numbers arriving at login and check against
      `EntityGroup.h` entries not yet handled.
- [ ] Alternatively: check if the Bajoran Favors screen is only populated after
      navigating to the Bajoran faction store — may not arrive at login.
- [ ] Once source is identified, cross-reference with `GlobalActiveBuffs` to
      populate `faction_favors` in `faction.json`.

### Feature: Daily goals / tasks
**Goal:** Export the player's current daily goals and their completion state so
AI assistants can suggest what to prioritise each day.
**Context:** `Resource_Daily_Meta1` and `Resource_Daily_Loyalty1` exist in the
mappings and may track daily progress as hidden tokens. Full goal definitions
and completion status likely live in a separate proto or EntityGroup.
**Investigation needed:**
- [ ] Identify which proto / EntityGroup carries daily goal definitions and
      progress — check for `DailyGoal`, `DailyTask`, `daily_goals`,
      `event_goals`, or `MissionDailyTask` in the JSON blob key list
- [ ] Check if `Resource_Daily_Meta1` / `Resource_Daily_Loyalty1` encode
      goal completion bitmask or are unrelated tracking tokens
- [ ] Determine if data arrives at login or requires navigating to the goals screen
- [ ] Export `daily_goals` section (or separate `daily_goals.json`) with
      goal name/id, type, progress, target, and completion status

### Feature: Event data � daily, weekly and other
**Goal:** Export current active events (daily goals, weekly missions, limited
time events, etc.) so AI assistants can factor them into planning advice.
**TODO:**
- [ ] Identify which data source carries active event state � candidates:
      `marauder_quick_scan_data` and `hazard_result_headers` seen in the
      JSON blob key list; also check EntityGroup types for event responses
- [ ] Distinguish event types: daily goals, weekly events, limited-time
      events, alliance events, and recurring events (e.g. Armada, Faction)
- [ ] Export `events` section to gamestate JSON with at minimum:
      event name, type (daily/weekly/other), active window (start/end),
      and player progress if available
- [ ] Determine whether data arrives at login or requires navigating to
      the events screen

### Feature: Alliance info and territory — PARTIALLY DONE
**What was implemented:**
- Alliance name, tag, level, member count and total power now exported as a
  structured `player.alliance` object in `player.json` (was a flat string before)
- Data comes from `AllianceProfiles` (type 71) which arrives automatically at login
**TODO (still open):**
- [ ] Territory holdings: which systems/zones the alliance controls, capture status,
      fortification level, and next capture window — likely requires navigating to
      the Territory screen; data source not yet identified
- [ ] Diplomacy state: war/peace/NAP relationships — `alliance_diplomacy_relationships`\r
      JSON key seen at login but not yet parsed or exported
### Feature: Refinery and faction store inventory
**Goal:** Export the player's current refinery inputs/outputs and faction
store stock so AI assistants can suggest optimal resources to gather and
which store purchases are within reach.
**Data wanted:**
- Refinery: which raw resources the player has available to refine,
  current refinery queue/slots, and output resource amounts
- Faction stores: per-faction store items, their costs, and whether the
  player has enough reputation + currency to buy them
**TODO:**
- [ ] Identify which data source carries refinery state � likely a
      dedicated EntityGroup type or a JSON blob key; check for
      `refinery`, `refinery_slots`, or `RefineryResponse` in the proto
- [ ] Identify faction store data source � likely `FactionStoreResponse`
      or similar EntityGroup; check proto definitions
- [ ] Export `refinery.json` with current refineable resources and queue
- [ ] Export `faction_stores.json` (or add to `faction.json`) with
      per-faction purchasable items and player's affordability

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
