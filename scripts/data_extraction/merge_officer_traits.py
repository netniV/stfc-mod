#!/usr/bin/env python3
"""
Merge officer trait IDs from officers.json into stfc_id_mappings.json

This script reads the full officers.json file which contains traitIds arrays,
and merges those trait IDs into the stfc_id_mappings.json file.
"""

import json
from pathlib import Path

def main():
    # Load officers.json
    officers_file = Path('game_data_maps/officers.json')
    if not officers_file.exists():
        print(f"Error: {officers_file} not found")
        return

    print(f"Loading {officers_file}...")
    with open(officers_file, 'r', encoding='utf-8') as f:
        officers_data = json.load(f)

    officers = officers_data.get('officers', [])
    print(f"Found {len(officers)} officers")

    # Load stfc_id_mappings.json
    mappings_file = Path('game_data_maps/stfc_id_mappings.json')
    if not mappings_file.exists():
        print(f"Error: {mappings_file} not found")
        return

    print(f"Loading {mappings_file}...")
    with open(mappings_file, 'r', encoding='utf-8') as f:
        mappings = json.load(f)

    # Merge trait IDs into officer mappings
    officers_in_mappings = mappings.get('officers', {})
    updated_count = 0
    added_traits_count = 0

    for officer in officers:
        officer_id = str(officer.get('id', ''))
        trait_ids = officer.get('traitIds', [])

        if not officer_id or not trait_ids:
            continue

        # Find this officer in the mappings
        if officer_id in officers_in_mappings:
            # Add trait_ids array
            officers_in_mappings[officer_id]['trait_ids'] = trait_ids
            updated_count += 1
            added_traits_count += len(trait_ids)
        else:
            # This officer isn't in our mappings yet - could happen with new officers
            print(f"Warning: Officer {officer_id} ({officer.get('name', 'unknown')}) not in mappings")

    # Save updated mappings
    print(f"\nUpdated {updated_count} officers with {added_traits_count} trait IDs")
    print(f"Saving to {mappings_file}...")

    with open(mappings_file, 'w', encoding='utf-8') as f:
        json.dump(mappings, f, indent=2, ensure_ascii=False)

    print("Done!")

if __name__ == '__main__':
    main()
