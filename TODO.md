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

### BUG-1: Full export fires too frequently, constantly wiping delta file
**Symptom:** `community_patch_gamestate_delta.json` appears empty or has only
1 entry even after changes. Full exports happen every few seconds.
**Root cause:** `capture_player_data()` calls `request_full_export()` on every
invocation — it fires repeatedly (twice per login). Every full export calls
`update_previous_state()` AND clears the delta file to `[]`. So differentials
get written then immediately wiped by the next forced full.
`capture_buildings()` also fires repeatedly with the same data, spamming
`request_immediate_export()`.
**Fix needed:**
- `capture_player_data`: only force full export when data meaningfully changes
  (name, ops_level, power actually differ from cached values), not on every call
- `capture_buildings`: same — skip `request_immediate_export()` if no values
  changed vs what's already cached
- Consider: full export should not clear the delta file — delta should
  accumulate until explicitly reset by the user or on a new session

### BUG-2: Peace shield detection incorrect ? (partially fixed)
**What's fixed:**
- Shield expiry now read from `StarbaseDetailedScan` (EntityGroup type=57)
- False SHIELD ALERT at startup suppressed until first scan received
- `active`, `expires_at`, `expires_epoch`, `seconds_remaining` all correct
- Peace shield changes tracked in differential export
- `shield_scan_received` flag prevents false alerts before first scan

**Trigger documented:** tap your station object on the system/galaxy map.
Interior view, exterior view, and system-button do NOT trigger the scan.

**Still pending:**
- [ ] Token count is always 0 — shield tokens arrive as
  `INVENTORYITEMTYPE_INVENTORYCONSUMABLE` (type=8) but we need to match
  them by `commonParams.refId` to the shield token resource IDs to count
  them correctly. Known token types: 1h(×3), 4h(×177), 8h(×166),
  12h(×117), 1day(×66), 3day(×4), 30day(×1), plus auto-10min shield.
- [ ] Golden peace shield (Scopely-applied after maintenance) — separate
  UI and countdown from regular shield; not yet identified in proto data.
- [ ] Document in startup instructions: tap your station on the system
  map to populate shield data (links to player.name/power investigation).

### BUG-3: Fleet/hangar ships not populating drydock assignments
**Symptom:** `drydocks` array is always empty `[]` in the gamestate JSON.
**Terminology (use consistently from now on):**
- **Fleet** = ships currently deployed/active (in a drydock slot, A–E)
- **Hangar** = all ships owned by the player (docked or otherwise)
**Fix needed:**
- Audit `capture_drydock_assignments()` — verify it is being called and with
  correct data; add log output showing what assignments arrive
- Check what game event/response triggers drydock data

---

## ?? Pending Work

### Player profile data population — documentation needed
**Context:** `player.name` and `player.power` are empty on fresh login until a
specific server response arrives. `player.ops_level` and `server` populate from
buildings data and are available immediately.
**TODO:**
- [x] Peace shield trigger identified: tap station on system/galaxy map view
- [x] Documented in INSTALL.md and GIST_SETUP_COMPLETE.md
- [ ] Identify exactly which in-game screens/actions trigger the player profile
      response that populates `name`, `power`, `server`, `alliance`
      (currently arrives automatically ~5s after login in testing, but not always)
- [ ] Identify what triggers fleet/drydock assignments (BUG-3)
- [ ] Update docs once BUG-3 resolved

### Feature 2: Battle log — outcome/ship empty on some entries
**Context:** `outcome` and `ship` fields in battlelog entries are resolved by
matching `Player Name` column in the CSV against `cached_player_data.name`.
If the player name hasn't arrived yet when the CSV is processed, these are empty.
**TODO:**
- [ ] Re-resolve outcome/ship lazily when player name becomes available, or
- [ ] Always populate from the combatants array directly (find the player's row
      by `player_id` config value rather than name string matching)

---

## ??? Stale / Superseded items (kept for reference)

- Ships ID mappings — 113 ships now in stfc_id_mappings.json ?
- Officer traits mappings — 84 traits in stfc_id_mappings.json ?
- Faction reputation export — implemented via Resource_FactionPoint_* ?
- Blueprint parts export — implemented via Resource_*_Parts_* ?
- Feature 3: Discord TC webhooks — moved to stfc-discord-tc-notifications repo


**Next Immediate Steps:**
1. Extract ships data from Spock's Club
2. Add ships to ID mappings
3. Enhance ship export with full data
4. Add faction reputation and blueprints
5. Test thoroughly
6. Open PR to upstream

---

**Great work so far! The foundation is solid and working beautifully. The remaining work is mostly data collection and hooking into existing game data structures.** ??
