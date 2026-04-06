# ?? STFC Community Mod - TODO List

## ? Completed (Feature 1: Gamestate Export)

### Core Export Functionality
- ? Basic gamestate export infrastructure
- ? Full snapshot export mode
- ? Differential export mode (changes only)
- ? Array-based delta accumulation
- ? Intelligent auto-switching (10% threshold)
- ? Skip export when no changes detected
- ? Export on startup + hourly full exports
- ? Configurable export intervals

### ID Mappings System
- ? ID mappings infrastructure (`id_mappings.h/cc`)
- ? MappingCache singleton pattern
- ? Load mappings from JSON file
- ? Enrich methods for all data types
- ? Officers mapping (277 officers) ? COMPLETE
- ? Buildings mapping (110 buildings) ? COMPLETE
- ? Resources mapping (4,613 resources) ? COMPLETE
- ? Research mapping (2,261 research items) ? COMPLETE
- ? Traits mapping (66 unique traits) ? COMPLETE

### Data Export Coverage
- ? Player metadata (ops level, name, alliance, power)
- ? Buildings (id, level, name)
- ? Research (id, level, name)
- ? Officers (id, rank, level, shards, name, faction, rarity, synergy, traits with names)
- ? Resources (id, amount, name, group, rarity)
- ? Ships (basic structure - id only)

### GitHub Integration
- ? Gist creation and configuration
- ? Sync script v2.0 (dual file sync)
- ? Real-time output (flush fix)
- ? Documentation (GIST_SETUP_COMPLETE.md)

### Documentation
- ? README updated with feature
- ? DIFFERENTIAL_EXPORT_DESIGN.md
- ? AI_SKILL_GAMESTATE_SYNC.md
- ? CONTRIBUTING_GAMESTATE_EXPORT.md
- ? game_data_maps/README.md

---

## ?? TODO: Complete Feature 1 (High Priority)

### 1. Complete ID Mappings ? HIGH PRIORITY

#### Ships Mapping (0 ships mapped)
- [ ] Find Spock's Club ships API endpoint or data source
- [ ] Extract ship IDs and names
- [ ] Add to `stfc_id_mappings.json`
- [ ] Implement `enrich_ship()` method fully
- [ ] Test with exported ship data

**Possible Sources:**
- Spock's Club ships page: https://spocks.club/ships/
- Check Network tab for AJAX/API calls
- May need to scrape HTML if no JSON endpoint

#### Officer Traits Mapping ? COMPLETE
- ? Find officer trait IDs in Spock's Club data (found in officers.json)
- ? Extract trait ID to name mappings (66 unique traits identified)
- ? Add traits section to mappings JSON (added to stfc_id_mappings.json)
- ? Enhance officer export to include trait names (implemented in id_mappings.cc)
- ? Update `enrich_officer()` to add traits (completed with trait names and descriptions)
- ? Create merge script to add traitIds from officers.json (merge_officer_traits.py)
- ? Merged 562 trait IDs across 248 officers

#### Command Center Avatars (0 avatars mapped)
- [ ] Extract avatar IDs from game data
- [ ] Map to avatar names/descriptions
- [ ] Add to mappings file
- [ ] Export player avatar selection

### 2. Enhanced Ship Data Export

Current ship export is minimal. Need to capture:
- [ ] Ship tier
- [ ] Ship level
- [ ] Ship scrapping info
- [ ] Ship location (station, traveling, destroyed)
- [ ] Ship crew assignments
- [ ] Hook into proper ship data structures in `sync.cc`

### 3. Faction Reputation Export

Currently placeholder. Need to:
- [ ] Hook into faction reputation data
- [ ] Export Federation, Klingon, Romulan points
- [ ] Include reputation level/tier if available
- [ ] Add to differential tracking

### 4. Blueprint Progress Export

Currently placeholder. Need to:
- [ ] Find blueprint progress data structures
- [ ] Export ship blueprints (current/required)
- [ ] Export officer shards (current/required to unlock)
- [ ] Track blueprint changes in differentials

---

## ?? TODO: Additional Features (Medium Priority)

### Feature 2: Enhanced Battle Log Export

- [ ] Extend existing battlelog sync in `sync.cc`
- [ ] Create `battlelog_export.cc/h` module
- [ ] Export structured JSON battle logs
  - [ ] Timestamp
  - [ ] Attacker/Defender names
  - [ ] Ship types
  - [ ] Outcome (win/loss)
  - [ ] Damage dealt/received
  - [ ] System/location
- [ ] Append to `community_patch_battlelog.json`
- [ ] Cap file at last 500 battles
- [ ] Add config option to enable/disable

### Feature 3: Alliance Discord Webhooks

- [ ] Territory capture timer reading
- [ ] Discord webhook POST integration (use `cpr` lib)
- [ ] Config options for webhook URL and reminder minutes
- [ ] Territory name and time remaining in message
- [ ] Discord embed formatting
- [ ] Add to config.cc

### Feature 4: Resource and Shield Alerts

- [ ] Read protected cargo capacity
- [ ] Compare with current resources
- [ ] Read shield status
- [ ] Read repair timer
- [ ] Log warnings when thresholds exceeded
- [ ] Optional: In-game overlay (advanced)

---

## ?? TODO: Data Extraction (Supporting Work)

### Spock's Club Data Collection

- [ ] Ships JSON extraction
  - Visit https://spocks.club/ships/
  - Open DevTools Network tab
  - Find JSON endpoint
  - Extract ship data

- [ ] Officer traits extraction
  - Check officers.json for trait IDs
  - Map trait IDs to trait names
  - Create trait mappings section

- [ ] Avatar data extraction
  - Find avatar IDs in game exports
  - Match to avatar names/images
  - Document in mappings

### Conversion Scripts

- [ ] Update `convert_spocks_data.py` for ships
- [ ] Add trait mapping conversion
- [ ] Add avatar mapping conversion
- [ ] Test merge functionality with new data types

---

## ?? TODO: Optimization & Polish

### Performance
- [ ] Profile export performance with large datasets
- [ ] Optimize diff calculation for huge resource lists
- [ ] Add export duration metrics to logs

### Error Handling
- [ ] Add retry logic for Gist sync failures
- [ ] Better error messages for mapping file issues
- [ ] Validate JSON structure before export

### Testing
- [ ] Test with multiple account states (new player vs advanced)
- [ ] Test differential exports over extended time
- [ ] Test sync script with network interruptions
- [ ] Verify all enrichment methods work correctly

### Configuration
- [ ] Add more TOML config options:
  - [ ] `export_mode` (full/differential/auto)
  - [ ] `full_export_interval`
  - [ ] `differential_export_interval`
  - [ ] `auto_mode_threshold`
  - [ ] Export file naming options

---

## ?? TODO: Merge & Release Preparation

### Code Quality
- [ ] Code review pass
- [ ] Remove any debug logging
- [ ] Ensure all code follows existing style
- [ ] Add code comments where needed
- [ ] Verify no hardcoded paths

### Documentation
- [ ] Update main README with full feature list
- [ ] Create user guide for gamestate export
- [ ] Document all config options
- [ ] Add troubleshooting section
- [ ] Screenshot examples for AI usage

### Git & PR Strategy
- [ ] Squash/clean up commit history if needed
- [ ] Write comprehensive PR description
- [ ] Test on clean install
- [ ] Get feedback from mod Discord
- [ ] Open PR to upstream: `netniV/stfc-mod:dev`

---

## ?? Priority Ranking

### P0 - Critical (Do First)
1. ? Core differential export (DONE)
2. ? GitHub Gist sync (DONE)
3. ? Ships ID mappings
4. ? Complete ship data export

### P1 - High (Do Soon)
5. ? Faction reputation export
6. ? Blueprint progress export
7. ? Officer traits mappings
8. ? Configuration options (TOML)

### P2 - Medium (Nice to Have)
9. ? Battle log export (Feature 2)
10. ? Avatar mappings
11. ? Performance optimization
12. ? Extended error handling

### P3 - Low (Future)
13. ? Discord webhooks (Feature 3)
14. ? Resource alerts (Feature 4)
15. ? In-game overlay alerts

---

## ?? Summary

**Completed:** ~70% of Feature 1 (Gamestate Export)
**Remaining for Feature 1:** Ships, traits, faction rep, blueprints
**Total Features Planned:** 4
**Overall Progress:** ~17% (Feature 1 done, 3 more to go)

**Next Immediate Steps:**
1. Extract ships data from Spock's Club
2. Add ships to ID mappings
3. Enhance ship export with full data
4. Add faction reputation and blueprints
5. Test thoroughly
6. Open PR to upstream

---

**Great work so far! The foundation is solid and working beautifully. The remaining work is mostly data collection and hooking into existing game data structures.** ??
