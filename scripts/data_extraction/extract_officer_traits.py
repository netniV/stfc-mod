#!/usr/bin/env python3
"""
Extract unique officer trait IDs from officers.json

This script analyzes the officers.json file and extracts all unique trait IDs.
It attempts to create a mapping by correlating trait IDs with officer properties.
"""

import json
from collections import defaultdict
from pathlib import Path

def main():
    officers_file = Path('game_data_maps/officers.json')

    if not officers_file.exists():
        print(f"Error: {officers_file} not found")
        return

    with open(officers_file, 'r', encoding='utf-8') as f:
        data = json.load(f)

    officers = data.get('officers', [])

    # Collect all unique trait IDs and which officers have them
    trait_to_officers = defaultdict(list)
    all_trait_ids = set()

    for officer in officers:
        trait_ids = officer.get('traitIds', [])
        all_trait_ids.update(trait_ids)

        for trait_id in trait_ids:
            trait_to_officers[trait_id].append({
                'name': officer.get('name', ''),
                'longname': officer.get('longname', ''),
                'faction': officer.get('faction', ''),
                'synergy': officer.get('synergy', ''),
                'rarity': officer.get('rarity', '')
            })

    print(f"Found {len(all_trait_ids)} unique trait IDs\n")

    # Analyze each trait
    print("Trait ID Analysis:")
    print("=" * 80)

    for trait_id in sorted(all_trait_ids):
        officers_with_trait = trait_to_officers[trait_id]
        count = len(officers_with_trait)

        # Analyze common properties
        factions = set(o['faction'] for o in officers_with_trait if o['faction'])
        synergies = set(o['synergy'] for o in officers_with_trait if o['synergy'])
        rarities = set(o['rarity'] for o in officers_with_trait if o['rarity'])

        print(f"\nTrait ID: {trait_id}")
        print(f"  Officers with this trait: {count}")

        if len(factions) == 1:
            print(f"  Faction-specific: {list(factions)[0]}")
        elif factions:
            print(f"  Factions: {', '.join(sorted(factions))}")

        if len(rarities) == 1:
            print(f"  Rarity-specific: {list(rarities)[0]}")
        elif rarities:
            print(f"  Rarities: {', '.join(sorted(rarities))}")

        if synergies:
            print(f"  Common synergies: {', '.join(sorted(synergies)[:5])}")

        # Show first few officers
        print(f"  Example officers: {', '.join(o['name'] for o in officers_with_trait[:5])}")

    # Export to JSON for manual mapping
    output = {
        "traits": {}
    }

    for trait_id in sorted(all_trait_ids):
        officers_with_trait = trait_to_officers[trait_id]
        output["traits"][str(trait_id)] = {
            "name": f"Trait_{trait_id}",  # Placeholder - needs manual identification
            "officer_count": len(officers_with_trait),
            "example_officers": [o['name'] for o in officers_with_trait[:10]]
        }

    output_file = Path('game_data_maps/officer_traits_template.json')
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(output, f, indent=2)

    print(f"\n\nTemplate mapping saved to: {output_file}")
    print("Please manually identify trait names and update the 'name' fields.")

if __name__ == '__main__':
    main()
