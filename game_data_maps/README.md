# STFC Game Data ID Mappings

This directory contains ID-to-name mappings extracted from Spock's Club for use with the gamestate export feature.

## Files

- `officers.json` - Raw officer data from Spock's Club (277 officers)
- `stfc_id_mappings.json` - Processed mappings ready for use by the mod

## How to Update

### 1. Collect Data from Spock's Club

Visit https://spocks.club and log in, then navigate to each page:

- Officers: https://spocks.club/officers/
- Research: https://spocks.club/research/
- Buildings: https://spocks.club/buildings/ and https://spocks.club/commandcenter/
- Ships: (need correct URL)
- Resources: https://spocks.club/resources/

For each page:
1. Open browser DevTools (F12)
2. Go to Network tab
3. Look for AJAX calls that return JSON data
4. Copy the JSON response
5. Save to `game_data_maps/<category>.json`

### 2. Convert to Mappings

```bash
cd C:\Users\Cord42\Projects\stfc-community-mod

# First category (creates new file)
python scripts\data_extraction\convert_spocks_data.py game_data_maps\officers.json --output game_data_maps\stfc_id_mappings.json

# Additional categories (merges into existing)
python scripts\data_extraction\convert_spocks_data.py game_data_maps\research.json --output game_data_maps\stfc_id_mappings.json --merge game_data_maps\stfc_id_mappings.json

python scripts\data_extraction\convert_spocks_data.py game_data_maps\buildings.json --output game_data_maps\stfc_id_mappings.json --merge game_data_maps\stfc_id_mappings.json
```

## Current Status

| Category | Count | Status |
|----------|-------|--------|
| Officers | 277   | ? Complete |
| Research | 0     | ? Pending |
| Buildings| 0     | ? Pending |
| Ships    | 0     | ? Pending |
| Resources| 0     | ? Pending |

## Data Format

The `stfc_id_mappings.json` file has this structure:

```json
{
  "officers": {
    "4219901626": {
      "name": "Officer 0718",
      "short_name": "0718",
      "faction": "Federation",
      "rarity": "Uncommon",
      "synergy": "AUXILIARY CONTROLS"
    }
  },
  "research": {},
  "buildings": {},
  "ships": {},
  "resources": {}
}
```

## Usage in Mod

The gamestate export feature will use this file to enrich the exported JSON with human-readable names:

```json
{
  "officers": [
    {
      "id": 988947581,
      "name": "James T. Kirk",  // <- Added from mappings
      "rank": 2,
      "level": 10
    }
  ]
}
```

## Legal Note

This data is extracted from Spock's Club (https://www.spocks.club) for use with the STFC Community Mod. The game data remains the property of Scopely and the Spock's Club maintainers.
