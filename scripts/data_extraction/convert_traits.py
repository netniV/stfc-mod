#!/usr/bin/env python3
"""
Convert Spock's Club traits.json to STFC ID mappings format

Usage:
    python convert_traits.py spocks_club_content_to_parse/traits.json
"""

import json
import sys
from pathlib import Path

def convert_traits(traits_data):
    """Convert traits data from Spock's Club format to our mapping format"""
    mappings = {}

    for trait in traits_data:
        trait_id = str(trait['id'])
        mappings[trait_id] = {
            'id': int(trait['id']),
            'name': trait['text'],
            'key': trait['key'],
            'source': 'spocks_club'
        }

    return mappings

def main():
    if len(sys.argv) < 2:
        print("Usage: python convert_traits.py <traits.json>")
        print("Example: python convert_traits.py spocks_club_content_to_parse/traits.json")
        return 1

    input_file = Path(sys.argv[1])

    if not input_file.exists():
        print(f"Error: File not found: {input_file}")
        return 1

    # Load traits
    print(f"?? Loading traits from: {input_file}")
    with open(input_file, 'r', encoding='utf-8') as f:
        traits_data = json.load(f)

    print(f"   Found {len(traits_data)} traits")

    # Convert to mappings format
    trait_mappings = convert_traits(traits_data)

    # Load existing mappings
    mappings_file = Path('game_data_maps/stfc_id_mappings.json')

    if mappings_file.exists():
        print(f"\n?? Loading existing mappings from: {mappings_file}")
        with open(mappings_file, 'r', encoding='utf-8') as f:
            all_mappings = json.load(f)
    else:
        print(f"\n??  Creating new mappings file")
        all_mappings = {
            'officers': {},
            'buildings': {},
            'research': {},
            'resources': {},
            'ships': {},
            'traits': {}
        }

    # Add/update traits
    if 'traits' not in all_mappings:
        all_mappings['traits'] = {}

    old_count = len(all_mappings['traits'])
    all_mappings['traits'].update(trait_mappings)
    new_count = len(all_mappings['traits'])

    print(f"   Added/updated {new_count - old_count} traits")
    print(f"   Total traits: {new_count}")

    # Save updated mappings
    print(f"\n?? Saving to: {mappings_file}")
    with open(mappings_file, 'w', encoding='utf-8') as f:
        json.dump(all_mappings, f, indent=2, ensure_ascii=False)

    # Summary
    print("\n" + "=" * 60)
    print("?? Final Summary:")
    print("=" * 60)
    for category, mappings in all_mappings.items():
        if mappings:
            print(f"   {category.capitalize():<12} {len(mappings):>5} entries")
    print("=" * 60)

    # Show sample traits
    print("\n?? Sample Traits:")
    for trait_id, trait in list(trait_mappings.items())[:10]:
        print(f"   ID {trait_id:>2}: {trait['name']}")

    print(f"\n? Successfully added traits to ID mappings!")
    return 0

if __name__ == '__main__':
    sys.exit(main())
