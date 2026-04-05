# Reverse Engineering Ships & Traits - Strategy

## Problem
- Spock's Club doesn't have ships or traits data yet
- next.spocksclub.com may have ships "coming soon"
- We need to map IDs to names for your ships and officer traits

## Solution: Reverse Engineering from Your Game Data

### Approach 1: Wait for Next Full Export
The mod captures ships (38 ships at 19:36:19 per logs), but the last full export was 19:13:36 (before ships were captured).

**Action:** Wait for next hourly full export (~20:13 PM) or trigger one by:
- Restarting the game (startup export)
- Waiting for hourly timer

### Approach 2: Manual Ship Identification
Once we have ships in the export, we can identify them by:

1. **Hull ID + Tier mapping**
   - Each ship has a `hull_id` and `tier`
   - Cross-reference with known ship progression
   - Example: Defiant has specific hull_id, tiers 1-9

2. **Resource name correlation**
   - Ship blueprints in resources have names
   - Example: `Resource_Parts_Battleship_G3` ? G3 Battleship parts
   - Match blueprint IDs to ship IDs

3. **Your manual identification**
   - You know which ships you own
   - Sort by tier/level in export
   - Match to your actual ships in-game

### Approach 3: Officer Traits Extraction
Officer traits are embedded in officer data structures.

**Options:**
1. Check if officer JSON from sync includes trait arrays
2. Look at protobuf definitions for trait fields
3. Extract trait IDs from your officers and manually map

## Action Plan

### Immediate (Tonight)
1. ? Created extraction script (`extract_my_data.py`)
2. ? Wait for next full export with ships data
3. ? Run extraction script on new export
4. ? Review extracted ship data

### Short Term (This Weekend)
1. Manual ship identification
   - Compare hull_id/tier to known ships
   - Use ship blueprint resources as hints
   - Create initial ship mappings for ships you own

2. Officer trait investigation
   - Check sync.cc for trait data capture
   - Look at protobuf definitions
   - Extract trait IDs if available

3. Create partial mappings file
   - `ships_partial.json` - ships you can identify
   - `traits_partial.json` - traits you can identify
   - Merge into `stfc_id_mappings.json`

### Medium Term (Next Week)
1. Community collaboration
   - Share extraction script with mod community
   - Crowdsource ship/trait identification
   - Build complete mapping over time

2. Monitor next.spocksclub.com
   - Check for ships data release
   - Integrate when available

## Current Status

### What We Have
- ? 38 ships being captured by mod (per logs)
- ? Officers being captured (per sync.cc hooks)
- ? Ship blueprint resources (21 types)
- ? Officer shard resources (9 types)
- ? Extraction script ready

### What We Need
- ? Fresh full export with ships/officers data
- ? Manual identification of ship names from hull_id
- ? Trait data extraction method
- ? Partial mappings file

## Sample Ship Identification Process

Once we have ships in export:

```json
{
  "id": 123456,
  "hull_id": 789,
  "tier": 5,
  "level": 23,
  "level_percentage": 0.35
}
```

**Identification steps:**
1. Check hull_id (unique per ship type)
2. Cross-reference tier (progression level)
3. Match to your known ships in-game
4. Create mapping entry:

```json
"ships": {
  "123456": {
    "name": "Defiant",
    "type": "Interceptor",
    "tier": 5,
    "hull_id": 789
  }
}
```

## Files Created

1. `scripts/data_extraction/extract_my_data.py` - Extraction script
2. `game_data_maps/reverse_engineered_data.json` - Output (partial, awaiting ships data)
3. This strategy doc

## Next Update

Check back after:
- Next full export (should be ~20:13 PM today)
- Or restart game for immediate startup export
