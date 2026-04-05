# Differential Gamestate Export Design

## Overview

This document outlines the design for implementing differential exports in the STFC Community Mod gamestate export feature. The goal is to minimize redundant data sent to GitHub Gist, reduce API calls, and optimize for AI assistant usage.

## Problem Statement

Currently, the gamestate export creates a full snapshot every time, even if only one building leveled up. This:
- Wastes bandwidth syncing unchanged data to GitHub Gist
- Makes it harder for AI assistants to quickly identify what changed
- Increases Gist file size and revision history bloat

## Solution: Differential Export with Change Tracking

### Export Modes

1. **Full Export** (`community_patch_gamestate.json`)
   - Complete snapshot of all game data
   - Created on startup or when requested
   - Used as baseline for differential exports

2. **Differential Export** (`community_patch_gamestate_delta.json`)
   - Contains only changed data since last export
   - Includes metadata about what changed
   - Smaller file size for efficient syncing

3. **Combined Export** (optional future enhancement)
   - Merges full + delta for AI assistants
   - Single file with full state + change annotations

### Data Structure

#### Full Export
```json
{
  "export_type": "full",
  "export_version": "1.1.0",
  "exported_at": "2026-04-04T10:00:00Z",
  "mod_version": "1.1.0",
  "meta": {
    "version": "1.0.0",
    "exported_at": "2026-04-04T10:00:00Z",
    "mod_version": "1.1.0",
    "mappings_loaded": true
  },
  "player": {...},
  "buildings": [...],
  "research": [...],
  "ships": [...],
  "officers": [...],
  "resources": {...}
}
```

#### Differential Export
```json
{
  "export_type": "differential",
  "export_version": "1.1.0",
  "exported_at": "2026-04-04T10:05:00Z",
  "base_export_at": "2026-04-04T10:00:00Z",
  "changes": {
    "buildings": {
      "modified": [
        {
          "id": 72,
          "name": "Mess Hall",
          "level": 47,
          "previous_level": 46,
          "changed_at": "2026-04-04T10:03:15Z"
        }
      ],
      "added": [],
      "removed": []
    },
    "research": {
      "modified": [
        {
          "id": 12345,
          "name": "Advanced Warp Theory",
          "level": 6,
          "previous_level": 5
        }
      ]
    },
    "resources": {
      "parsteel": {
        "current": 1500000,
        "previous": 1200000,
        "delta": 300000
      }
    },
    "ships": {...},
    "officers": {...}
  },
  "summary": {
    "total_changes": 3,
    "categories_changed": ["buildings", "research", "resources"]
  }
}
```

### Implementation Approach

#### Phase 1: Change Detection Infrastructure

1. **Add previous state tracking**
   ```cpp
   static std::unordered_map<int64_t, int32_t> previous_buildings;
   static std::unordered_map<int64_t, int32_t> previous_research;
   // etc. for all data types
   ```

2. **Implement diff calculation**
   ```cpp
   json calculate_buildings_diff();
   json calculate_research_diff();
   // etc.
   ```

3. **Add change timestamps**
   - Track when each item was last modified
   - Include in differential export

#### Phase 2: Export Modes

1. **Config options**
   ```toml
   [gamestate_export]
   enabled = true
   export_mode = "auto"  # "full", "differential", "auto"
   full_export_interval = 3600  # Full export every hour
   delta_export_interval = 300  # Differential every 5 minutes
   ```

2. **Auto mode logic**
   - Export differential if < 10% of data changed
   - Export full if >= 10% changed or hourly timer expires
   - Export full on startup

#### Phase 3: GitHub Gist Optimization

1. **Update sync script** (`scripts/sync_to_gist.py`)
   - Handle both full and differential files
   - Merge differential into cached full state
   - Expose merged state to AI assistants

2. **File strategy**
   - Gist contains two files:
     - `stfc_gamestate_full.json` - Latest full export
     - `stfc_gamestate_delta.json` - Latest differential
   - AI assistants fetch full + apply delta if needed

### Configuration

Add to `community_patch_settings.toml`:

```toml
[gamestate_export]
enabled = true
export_on_startup = true

# Export intervals (seconds, 0 = disabled)
full_export_interval = 3600      # Full snapshot every hour
differential_export_interval = 300  # Changes every 5 minutes

# Export mode: "full", "differential", "auto"
# - full: Always export complete snapshots
# - differential: Always export only changes (requires full baseline)
# - auto: Intelligent switching based on change percentage
export_mode = "auto"
auto_mode_threshold = 0.10  # 10% change triggers full export

# File paths (empty = game directory)
export_path = ""
full_export_filename = "community_patch_gamestate.json"
differential_export_filename = "community_patch_gamestate_delta.json"
```

### Benefits

1. **Reduced bandwidth**: Only sync what changed
2. **Faster AI parsing**: AI sees exactly what's new
3. **Historical tracking**: Git diffs show clear progression
4. **Efficient Gist usage**: Smaller updates, less API calls
5. **Better debugging**: Easy to see what triggered an export

### Migration Path

1. **v1.0 (Current)**: Full exports only
2. **v1.1 (Phase 1)**: Add change detection, dual export (full + delta)
3. **v1.2 (Phase 2)**: Auto mode with intelligent switching
4. **v2.0 (Future)**: Advanced features (compression, encryption, cloud sync)

### Compatibility

- Existing `sync_to_gist.py` continues to work with full exports
- New version of sync script handles both modes
- AI assistants can use either full or differential data
- No breaking changes to existing installations

## Open Questions

1. Should we compress differential exports (gzip)?
2. How many previous states to track (for multi-level diff)?
3. Should we add rollback capability (apply diff in reverse)?
4. Export format versioning strategy?

## Next Steps

1. ? Design document (this file)
2. ? Implement change detection infrastructure
3. ? Add differential export generation
4. ? Update sync_to_gist.py script
5. ? Add config options
6. ? Testing and documentation
7. ? PR to upstream

