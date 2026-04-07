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

### Player profile data population � documentation needed
**Context:** `player.name` and `player.power` are empty on fresh login until a
specific server response arrives. `player.ops_level` and `server` populate from
buildings data and are available immediately.
**TODO:**
- [x] Peace shield trigger identified: tap station on system view
- [x] Documented in INSTALL.md and GIST_SETUP_COMPLETE.md
- [x] Drydock assignments arrive automatically at login (no navigation needed)
- [ ] Identify exactly which in-game screens/actions trigger the player profile
      response that populates `name`, `power`, `server`, `alliance`
      (currently arrives automatically ~5s after login in testing, but not always)
- [ ] Update docs once player profile trigger is fully confirmed

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

### Feature: Ship upgrade resource planning
**Goal:** For each ship in the player's hangar, export what resources are
needed to reach the next tier/level so AI assistants can give specific
upgrade advice.
**TODO:**
- [ ] Identify where ship upgrade cost data lives � likely
      `ShipTierSpecs` (EntityGroup type=50) or `BaseShipTierSpecs` (type=49)
- [ ] Cross-reference each ship's current tier/level from `cached_ships`
      against the spec data to compute what's needed for next upgrade
- [ ] Add `upgrade_cost` block to each ship entry in the gamestate JSON
      (or as a separate top-level `ship_upgrade_costs` section)
- [ ] Only include ships the player actually owns

### Feature: Faction store tokens and armada credits — DONE
**What was found:** `Resource_FactionToken_*` (store currencies) and
`Resource_Faction_SArmada_Credit_*` / `Resource_Faction_SArmadaDir_*`
(armada directives) were already in `cached_resources` at login.
No `Resource_FactionFavor_*` pattern exists — 'favour' in STFC is
represented by the `FactionToken` resources.
**Implemented:** `faction_store_tokens` and `armada_credits` sections added
to `faction.json`. Only non-zero amounts included. 12 token types and
6 armada credit types exported. Tests: 55/55.

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

### Feature: Alliance info and territory
**Goal:** Export alliance details and territory holdings so AI assistants
can help with territory capture strategy and co-ordination planning.
**Data wanted:**
- Alliance name, tag, member count, level, total power
- Territory holdings: which systems/zones the alliance controls,
  capture status, fortification level, and any active contests
- Territory capture windows (when zones are capturable)
- Alliance diplomacy state (war/peace/NAP relationships) � already
  partially in the `alliance_diplomacy_relationships` JSON key
**TODO:**
- [ ] Audit what already arrives in the JSON blob � known candidates:
      `alliance`, `alliance_container`, `alliance_members`,
      `alliance_member_activity`, `alliance_diplomacy_relationships`,
      `alliances_info` (all seen in the key list at login)
- [ ] Identify which key/proto carries territory holdings and capture
      windows � likely requires navigating to the Territory screen
- [ ] Export `alliance` section to gamestate JSON (name, tag, level,
      member count, power)
- [ ] Export `territory` section: controlled zones with fortification
      level and next capture window
- [ ] Export `diplomacy` section: war/peace/NAP relationships with
      other alliances

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
