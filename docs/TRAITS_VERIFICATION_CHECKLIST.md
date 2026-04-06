# Traits/Synergies Integration - Quick Verification Guide

## What to Check After Next Export

After running the game with the updated mod, check `community_patch_gamestate.json` for:

### 1. Officer Traits Array

Look for the `traits` array in officer objects:

```json
{
  "officers": [
    {
      "id": 4219901626,
      "name": "Officer 0718",
      "rank": 3,
      "level": 15,
      "shards": 0,
      "faction": "Federation",
      "rarity": "Uncommon",
      "synergy": "AUXILIARY CONTROLS",
      "traits": [
        {
          "id": 2907922750,
          "name": "Base Officer Trait",
          "description": "Common trait present in most officers"
        }
      ]
    }
  ]
}
```

### 2. Verify Trait Loading in Logs

Check `community_patch.log` for these log entries on startup:

```
Loaded 248 officer name mappings
Loaded 66 trait name mappings
Successfully loaded ID mappings from <path>\stfc_id_mappings.json
```

### 3. Sample Officers to Check

**Officer 0718** (ID: 4219901626)
- Should have 1 trait: "Base Officer Trait"

**Ahvix** (ID: 229898163)
- Should have 2 traits: "Base Officer Trait", "Engineer"

**Kirk / Picard / Other Epic Officers**
- Should have 2-3 traits including "Captain" or "Epic Commander"

### 4. Differential Export

The differential export should also include traits when officers change:

```json
{
  "export_type": "differential",
  "changes": {
    "officers": {
      "modified": [
        {
          "id": 988947581,
          "level": 11,  // Changed
          "traits": [...]  // Full trait array included
        }
      ]
    }
  }
}
```

## If Traits Don't Appear

1. **Check mappings file exists:**
   ```
   C:\Games\Star Trek Fleet Command\...\game\stfc_id_mappings.json
   ```

2. **Verify it has traits section:**
   - Open the file and look for `"traits": {`

3. **Check mod logs:**
   - Look for "Loaded X trait name mappings"
   - If it says 0, the traits section wasn't loaded

4. **Rebuild and re-copy:**
   ```bash
   xmake build
   copy build\windows\x64\debug\stfc-community-mod.dll "C:\Games\...\version.dll"
   ```

## Testing Checklist

- [ ] Traits appear in full export
- [ ] Traits appear in differential export
- [ ] Trait names are human-readable
- [ ] Trait descriptions are present
- [ ] Multiple traits per officer work correctly
- [ ] Synergies still appear (not broken by traits addition)
- [ ] No performance impact on export speed

## Performance Note

The trait enrichment adds minimal overhead:
- Trait lookup is O(1) hash map access
- Only done during export (not real-time)
- Typical export processes 200-300 officers in <1 second

---

**Ready for testing!** ??
