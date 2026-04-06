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

### BUG-1: Full export fires too frequently, constantly wiping delta file — FIXED
**Was:** `capture_player_data/buildings/ships` called `request_full_export()` /
`request_immediate_export()` unconditionally on every call. Game sends repeated
identical payloads at login, flooding full exports that wiped the delta file.
**Fix:** All three capture functions now compare incoming values against cached
state and only request an export when something actually changed.

### BUG-2: Peace shield detection incorrect — FULLY FIXED
**What's fixed:**
- Shield expiry read from `StarbaseDetailedScan` (EntityGroup type=57)
- False SHIELD ALERT at startup suppressed until first scan received
- `active`, `expires_at`, `expires_epoch`, `seconds_remaining` all correct
- Token count read from `cached_resources` using known resource IDs — verified
  534 tokens (1h×3 + 4h×177 + 8h×166 + 12h×117 + 1d×66 + 3d×4 + 30d×1)
- Peace shield changes tracked in differential export

**Trigger documented:** tap your station in the **system view** of your home
system. NOT the galaxy view, NOT interior/exterior button, NOT system button.
Documented in INSTALL.md and GIST_SETUP_COMPLETE.md.

**Still open (follow-up):**
- [ ] Golden peace shield (Scopely-applied after maintenance) — separate UI
      and countdown; not yet identified in proto data
- [ ] Auto-10min shield (triggered on first attack when unshielded) — verify
      it shows correctly via the same `StarbaseDetailedScan` path

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
