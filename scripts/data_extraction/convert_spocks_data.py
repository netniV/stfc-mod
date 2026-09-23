#!/usr/bin/env python3
"""
Convert Spock's Club data format to STFC ID mappings

Takes the JSON data from Spock's Club and converts it to a simplified
mapping format for use with the gamestate export feature.

Usage:
    python convert_spocks_data.py officers.json --output stfc_mappings.json
"""

import argparse
import json
import sys
from pathlib import Path

def convert_officers(data):
    """Convert officer data from Spock's Club format to our mapping format"""
    mappings = {}

    # Handle both formats: {"officers": [...]} and [...]
    officers = data if isinstance(data, list) else data.get('officers', [])

    for officer in officers:
        officer_id = str(officer['id'])
        mappings[officer_id] = {
            'name': officer.get('longname', officer.get('name', f'Officer {officer_id}')),
            'short_name': officer.get('name', ''),
            'faction': officer.get('faction', ''),
            'rarity': officer.get('rarity', ''),
            'synergy': officer.get('synergy', '')
        }

        # Add abilities if present and not empty
        if officer.get('abilities'):
            mappings[officer_id]['abilities'] = officer['abilities']

    return mappings

def main():
    parser = argparse.ArgumentParser(
        description='Convert Spock\'s Club data to STFC ID mappings'
    )
    parser.add_argument(
        'input_file',
        type=Path,
        help='JSON file containing Spock\'s Club officer data'
    )
    parser.add_argument(
        '--output', '-o',
        type=Path,
        default=Path('stfc_id_mappings.json'),
        help='Output file (default: stfc_id_mappings.json)'
    )
    parser.add_argument(
        '--merge',
        type=Path,
        help='Merge with existing mappings file'
    )

    args = parser.parse_args()

    # Load existing mappings if merging
    all_mappings = {
        'officers': {},
        'research': {},
        'buildings': {},
        'ships': {},
        'resources': {},
        'components': {}
    }

    if args.merge and args.merge.exists():
        print(f"Loading existing mappings from: {args.merge}")
        with open(args.merge, 'r', encoding='utf-8') as f:
            existing = json.load(f)
            for category in all_mappings.keys():
                if category in existing:
                    all_mappings[category].update(existing[category])
        print()

    # Load input file
    print(f"Processing: {args.input_file}")
    try:
        with open(args.input_file, 'r', encoding='utf-8') as f:
            data = json.load(f)
    except UnicodeDecodeError:
        # Try with utf-8-sig to handle BOM or other encodings
        with open(args.input_file, 'r', encoding='utf-8-sig', errors='replace') as f:
            data = json.load(f)

    # Convert officers
    officer_mappings = convert_officers(data)
    all_mappings['officers'].update(officer_mappings)
    print(f"  Converted {len(officer_mappings)} officers")

    # Write output
    print(f"\nWriting mappings to: {args.output}")
    with open(args.output, 'w', encoding='utf-8') as f:
        json.dump(all_mappings, f, indent=2, ensure_ascii=False)

    # Summary
    print("\n" + "=" * 60)
    print("Summary:")
    print("=" * 60)
    for category, mappings in all_mappings.items():
        if mappings:
            print(f"  {category.capitalize()}: {len(mappings):>5} entries")
    print("=" * 60)

    return 0

if __name__ == '__main__':
    sys.exit(main())
