#!/usr/bin/env python3
"""
Process Spock's Club API data into STFC ID mappings format

Converts the raw API data into the format used by the mod.
"""

import json
from pathlib import Path
from collections import defaultdict

def process_ships(ships_data):
    """Process ships data from API format to mapping format"""
    ships_by_id = defaultdict(dict)

    # The ships data has multiple entries per ship (name, description, abilities, etc.)
    # We need to extract just the ship name entries
    for entry in ships_data:
        ship_id = entry.get('id')
        key = entry.get('key', '')
        text = entry.get('text', '')

        # Ship names are in 'ship_name_X' keys
        if 'ship_name_' in key and ship_id:
            ships_by_id[ship_id]['name'] = text
            ships_by_id[ship_id]['id'] = ship_id

        # Try to extract class from description or other fields
        # (We may need to enhance this with more data)

    return ships_by_id

def process_traits(traits_data):
    """Process traits data from API format to mapping format"""
    traits_by_id = {}

    for entry in traits_data:
        trait_id = entry.get('id')
        key = entry.get('key', '')
        text = entry.get('text', '')

        # Trait names are in 'trait_name_X' keys
        if 'trait_name_' in key and trait_id:
            traits_by_id[trait_id] = {
                'name': text,
                'description': f'{text} officer trait'
            }

    return traits_by_id

def main():
    print("Processing Spock's Club API Data")
    print("=" * 60)

    # Load raw data
    raw_file = Path('spocks_club_content_to_parse/api_data_raw.json')
    if not raw_file.exists():
        print(f"Error: {raw_file} not found. Run fetch_spocks_api_data.py first.")
        return

    with open(raw_file, 'r', encoding='utf-8') as f:
        raw_data = json.load(f)

    # Process ships
    print("\nProcessing ships...")
    ships_data = raw_data.get('ships', [])
    ships_mapping = process_ships(ships_data)
    print(f"  Processed {len(ships_mapping)} unique ships")

    # Process traits
    print("\nProcessing traits...")
    traits_data = raw_data.get('traits', [])
    traits_mapping = process_traits(traits_data)
    print(f"  Processed {len(traits_mapping)} unique traits")

    # Load existing mappings
    mappings_file = Path('game_data_maps/stfc_id_mappings.json')
    print(f"\nLoading existing mappings from {mappings_file}...")

    with open(mappings_file, 'r', encoding='utf-8') as f:
        existing_mappings = json.load(f)

    # Update ships
    print("\nUpdating ships in mappings...")
    if 'ships' not in existing_mappings:
        existing_mappings['ships'] = {}

    for ship_id, ship_data in ships_mapping.items():
        existing_mappings['ships'][ship_id] = {
            'name': ship_data['name'],
            'class': '',  # Will need additional data source for this
            'source': 'spocks_club_api'
        }

    print(f"  Updated {len(ships_mapping)} ships")

    # Update traits with official names
    print("\nUpdating traits in mappings...")
    if 'traits' not in existing_mappings:
        existing_mappings['traits'] = {}

    for trait_id, trait_data in traits_mapping.items():
        existing_mappings['traits'][trait_id] = {
            'name': trait_data['name'],
            'description': trait_data['description']
        }

    print(f"  Updated {len(traits_mapping)} traits")

    # Save updated mappings
    print(f"\nSaving updated mappings to {mappings_file}...")
    with open(mappings_file, 'w', encoding='utf-8') as f:
        json.dump(existing_mappings, f, indent=2, ensure_ascii=False)

    print("\nDone!")
    print("\nSummary:")
    print(f"  Ships: {len(existing_mappings.get('ships', {}))}")
    print(f"  Traits: {len(existing_mappings.get('traits', {}))}")
    print(f"  Officers: {len(existing_mappings.get('officers', {}))}")
    print(f"  Research: {len(existing_mappings.get('research', {}))}")
    print(f"  Buildings: {len(existing_mappings.get('buildings', {}))}")
    print(f"  Resources: {len(existing_mappings.get('resources', {}))}")

    # Show sample ships
    print("\nSample ships added:")
    for ship_id, ship_data in list(ships_mapping.items())[:10]:
        print(f"  {ship_id}: {ship_data['name']}")

    # Show sample traits
    print("\nSample traits updated:")
    for trait_id, trait_data in list(traits_mapping.items())[:10]:
        print(f"  {trait_id}: {trait_data['name']}")

if __name__ == '__main__':
    main()
