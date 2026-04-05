#!/usr/bin/env python3
"""
STFC.space Ships Data Scraper - Using Official API

Uses the stfc.space backend API at https://staging-api.stfc.dev/v1/
"""

import requests
import json
from pathlib import Path

OUTPUT_FILE = Path(__file__).parent.parent.parent / "game_data_maps" / "ships_from_stfc_space.json"
API_BASE = "https://staging-api.stfc.dev/v1"

def fetch_ships():
    """Fetch all ships from stfc.space API"""
    url = f"{API_BASE}/ship"

    print(f"?? Fetching ships from {url}")

    try:
        response = requests.get(url, params={'n': 'web-main'})

        if response.status_code != 200:
            print(f"? HTTP {response.status_code}: {response.text}")
            return None

        ships = response.json()
        print(f"? Received {len(ships)} ships")

        return ships

    except Exception as e:
        print(f"? Error: {e}")
        return None

def fetch_all_data():
    """Fetch all game data (ships, officers, buildings, research, resources)"""
    endpoints = {
        'ships': '/ship',
        'officers': '/officer',
        'buildings': '/building',
        'research': '/research',
        'resources': '/resource'
    }

    all_data = {}

    for category, endpoint in endpoints.items():
        url = f"{API_BASE}{endpoint}"
        print(f"\n?? Fetching {category} from {url}")

        try:
            response = requests.get(url, params={'n': 'web-main'})

            if response.status_code == 200:
                data = response.json()
                print(f"? Received {len(data)} {category}")
                all_data[category] = data
            else:
                print(f"??  HTTP {response.status_code} for {category}")
                all_data[category] = []

        except Exception as e:
            print(f"? Error fetching {category}: {e}")
            all_data[category] = []

    return all_data

def process_ships_data(ships_raw):
    """Process raw ships data into our mapping format"""
    ships_map = {}

    for ship in ships_raw:
        ship_id = ship['id']

        # Map hull_type to readable string
        hull_types = {
            0: "Explorer",
            1: "Interceptor", 
            2: "Battleship",
            3: "Survey"
        }

        ships_map[str(ship_id)] = {
            'id': ship_id,
            'name': ship.get('name', f'Ship_{ship_id}'),
            'hull_id': ship_id,
            'hull_type': hull_types.get(ship.get('hull_type'), 'Unknown'),
            'max_tier': ship.get('max_tier'),
            'rarity': ship.get('rarity'),
            'faction': ship.get('faction'),
            'source': 'stfc.space_api'
        }

    return ships_map

def main():
    print("?? STFC.space Official API Scraper")
    print("=" * 60)

    # Fetch all data
    all_data = fetch_all_data()

    if not all_data.get('ships'):
        print("\n? Failed to fetch ships data")
        return

    # Process ships
    ships_map = process_ships_data(all_data['ships'])

    # Save ships to file
    OUTPUT_FILE.parent.mkdir(exist_ok=True)
    with open(OUTPUT_FILE, 'w') as f:
        json.dump(ships_map, f, indent=2)

    print(f"\n? Saved {len(ships_map)} ships to {OUTPUT_FILE}")

    # Show sample
    print("\n?? Sample ships:")
    for ship_id, ship in list(ships_map.items())[:10]:
        print(f"   {ship['name']} (ID: {ship_id}, Type: {ship['hull_type']}, Max Tier: {ship['max_tier']})")

    # Also save all data for reference
    all_data_file = OUTPUT_FILE.parent / "all_stfc_space_data.json"

    # Convert to mappings format
    all_mappings = {
        'ships': ships_map,
        'officers': {str(o['id']): {'id': o['id'], 'name': o.get('name', f"Officer_{o['id']}"), 'source': 'stfc.space_api'} for o in all_data.get('officers', [])},
        'buildings': {str(b['id']): {'id': b['id'], 'name': b.get('name', f"Building_{b['id']}"), 'source': 'stfc.space_api'} for b in all_data.get('buildings', [])},
        'research': {str(r['id']): {'id': r['id'], 'name': r.get('name', f"Research_{r['id']}"), 'source': 'stfc.space_api'} for r in all_data.get('research', [])},
        'resources': {str(res['id']): {'id': res['id'], 'name': res.get('name', f"Resource_{res['id']}"), 'source': 'stfc.space_api'} for res in all_data.get('resources', [])}
    }

    with open(all_data_file, 'w') as f:
        json.dump(all_mappings, f, indent=2)

    print(f"\n? Saved all data to {all_data_file}")
    print(f"   Officers: {len(all_mappings['officers'])}")
    print(f"   Buildings: {len(all_mappings['buildings'])}")
    print(f"   Research: {len(all_mappings['research'])}")
    print(f"   Resources: {len(all_mappings['resources'])}")

    print("\n?? Next steps:")
    print("   1. Merge ships_from_stfc_space.json into stfc_id_mappings.json")
    print("   2. Test enriched exports with ship names")
    print("   3. Consider merging all_stfc_space_data.json to get complete mappings")

if __name__ == "__main__":
    main()
