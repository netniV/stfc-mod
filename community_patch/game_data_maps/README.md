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
3. Reload the page or interact with it
4. Look for AJAX calls that return JSON data (check XHR filter)
5. Click on the request to view the response
6. Right-click ? Copy ? Copy response
7. Save to `game_data_maps/<category>.json` or `spocks_club_content_to_parse/<category>.json`

### Finding the Right API Calls

**Research**: The research page uses DataTables which loads data via AJAX. Look for requests to endpoints containing "research" or "datatables" in the Network tab when the page loads.

**Ships**: Similar to research - check Network tab when viewing ships page.

**Officer Traits**: May be embedded in the officers.json data or require a separate endpoint.

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

| Category | Count | Status | Notes |
|----------|-------|--------|-------|
| Officers | 277   | ? Complete | Full data from Spock's Club JSON |
| Buildings| 110   | ? Complete | Full data from Spock's Club JSON |
| Resources| 4613  | ? Complete | Full data from Spock's Club inventory.json |
| Research | 2261  | ? Complete | Extracted from HTML table (unique research IDs) |
| Ships    | 0     | ? Pending | Need to find API endpoint or JSON export |
| Traits   | 0     | ? Pending | Officer trait IDs not found in exports |
| Avatars  | 0     | ? Pending | Command center avatar IDs not in HTML exports |

**Total: 7,261 game items mapped!**

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
