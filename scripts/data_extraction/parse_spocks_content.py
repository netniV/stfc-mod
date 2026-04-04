#!/usr/bin/env python3
"""
Parse Spock's Club HTML/JSON files to extract ID mappings

Processes the downloaded HTML and JSON files from Spock's Club
to extract research, buildings, resources, and command center data.

Usage:
    python parse_spocks_content.py
"""

import json
import sys
from pathlib import Path
from html.parser import HTMLParser
from typing import Dict, List, Any

class ResearchHTMLParser(HTMLParser):
    """Parse research.html to extract research ID mappings"""

    def __init__(self):
        super().__init__()
        self.in_row = False
        self.current_research = {}
        self.current_td_index = 0
        self.current_data = []
        self.research_map = {}

    def handle_starttag(self, tag, attrs):
        if tag == 'tr':
            attrs_dict = dict(attrs)
            if 'data-rid' in attrs_dict:
                # Only capture first occurrence of each research (myrow="0" or first seen)
                research_id = attrs_dict.get('data-rid')
                myrow = attrs_dict.get('data-myrow', '-1')

                # Only process myrow="0" and if we haven't seen this research ID yet
                if myrow == '0' and research_id not in self.research_map:
                    self.in_row = True
                    self.current_research = {
                        'id': research_id,
                        'tree_id': attrs_dict.get('data-treeid'),
                        'title': attrs_dict.get('title', '')
                    }
                    self.current_td_index = 0
                    self.current_data = []
        elif tag == 'td' and self.in_row:
            pass  # Just increment index on close

    def handle_endtag(self, tag):
        if tag == 'tr' and self.in_row:
            # Process completed row
            if len(self.current_data) >= 3:
                research_id = self.current_research['id']
                name = self.current_data[0].strip()
                category = self.current_data[2].strip() if len(self.current_data) > 2 else ''

                if research_id and name:
                    self.research_map[research_id] = {
                        'name': name,
                        'category': category,
                        'description': self.current_research['title']
                    }

            self.in_row = False
            self.current_research = {}
            self.current_data = []
            self.current_td_index = 0
        elif tag == 'td' and self.in_row:
            self.current_td_index += 1

    def handle_data(self, data):
        if self.in_row:
            self.current_data.append(data)

class CommandCenterHTMLParser(HTMLParser):
    """Parse command center HTML files to extract avatar/command center IDs"""

    def __init__(self):
        super().__init__()
        self.avatar_id = None
        self.avatar_name = None

    def handle_starttag(self, tag, attrs):
        attrs_dict = dict(attrs)

        # Look for avatar selection dropdown
        if tag == 'select' and 'data-setting-key' in attrs_dict:
            if attrs_dict.get('data-setting-key') == 'avatar':
                pass  # Will get options next

        # Look for option with selected attribute
        if tag == 'option' and 'selected' in attrs_dict:
            if 'value' in attrs_dict:
                self.avatar_id = attrs_dict['value']

    def handle_data(self, data):
        if self.avatar_id and not self.avatar_name:
            self.avatar_name = data.strip()

def parse_buildings_json(file_path: Path) -> Dict[str, Dict]:
    """Parse buildings.json"""
    print(f"Parsing buildings from: {file_path}")

    with open(file_path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    buildings = data.get('buildings', [])
    mapping = {}

    for building in buildings:
        building_id = str(building['id'])
        mapping[building_id] = {
            'name': building['text'],
            'key': building.get('key', '')
        }

    print(f"  Found {len(mapping)} buildings")
    return mapping

def parse_inventory_json(file_path: Path) -> Dict[str, Dict]:
    """Parse inventory.json for resources"""
    print(f"Parsing resources from: {file_path}")

    with open(file_path, 'r', encoding='utf-8-sig', errors='replace') as f:
        data = json.load(f)

    resources = data if isinstance(data, list) else []
    mapping = {}

    for resource in resources:
        resource_id = str(resource['id'])
        mapping[resource_id] = {
            'name': resource.get('id_str', resource.get('name', '')),
            'resource_key': resource.get('resource_id', ''),
            'rarity': resource.get('rarity', 0),
            'group': resource.get('resource_group_id_str', ''),
            'subtype': resource.get('subtype', 0)
        }

    print(f"  Found {len(mapping)} resources")
    return mapping

def parse_research_html(file_path: Path) -> Dict[str, Dict]:
    """Parse research.html by splitting into rows"""
    print(f"Parsing research from: {file_path}")

    import re

    with open(file_path, 'r', encoding='utf-8', errors='replace') as f:
        html_content = f.read()

    # Split by </tr> to get individual rows
    rows = html_content.split('</tr>')

    mapping = {}

    for row in rows:
        # Check if this row has data-myrow="0"
        if 'data-myrow="0"' not in row:
            continue

        # Extract research ID
        rid_match = re.search(r'data-rid="(\d+)"', row)
        if not rid_match:
            continue

        research_id = rid_match.group(1)

        # Extract all TD contents
        td_matches = re.findall(r'<td[^>]*?>(.*?)</td>', row, re.DOTALL)

        if td_matches and len(td_matches) > 0:
            name = td_matches[0].strip()
            # Remove HTML tags and normalize whitespace
            name = re.sub(r'<[^>]+>', '', name)
            name = re.sub(r'\s+', ' ', name).strip()

            if name:
                # Get category (3rd TD)
                category = ''
                if len(td_matches) > 2:
                    category = td_matches[2].strip()
                    category = re.sub(r'<[^>]+>', '', category).strip()

                mapping[research_id] = {
                    'name': name,
                    'category': category
                }

    print(f"  Found {len(mapping)} research items")
    return mapping

def parse_command_center_html(directory: Path) -> Dict[str, Dict]:
    """Parse command center HTML files for avatars"""
    print(f"Parsing command center avatars from: {directory}")

    avatars = {}

    for html_file in directory.glob('*.html'):
        with open(html_file, 'r', encoding='utf-8', errors='replace') as f:
            content = f.read()

        parser = CommandCenterHTMLParser()
        parser.feed(content)

        if parser.avatar_id:
            # Extract name from filename if not found in HTML
            name = parser.avatar_name or html_file.stem.replace('_', ' ').title()

            avatars[parser.avatar_id] = {
                'name': name,
                'key': html_file.stem
            }

    print(f"  Found {len(avatars)} command center avatars")
    return avatars

def main():
    # Base directory
    base_dir = Path(__file__).parent.parent.parent / 'spocks_club_content_to_parse'

    if not base_dir.exists():
        print(f"ERROR: Directory not found: {base_dir}")
        return 1

    # Output file
    output_file = Path(__file__).parent.parent.parent / 'game_data_maps' / 'stfc_id_mappings.json'

    # Load existing mappings
    all_mappings = {
        'officers': {},
        'research': {},
        'buildings': {},
        'ships': {},
        'resources': {},
        'components': {},
        'avatars': {}
    }

    if output_file.exists():
        print(f"Loading existing mappings from: {output_file}\n")
        with open(output_file, 'r', encoding='utf-8') as f:
            existing = json.load(f)
            for category in all_mappings.keys():
                if category in existing:
                    all_mappings[category] = existing[category]

    # Parse each file/directory
    try:
        # Buildings
        buildings_file = base_dir / 'buildings.json'
        if buildings_file.exists():
            all_mappings['buildings'] = parse_buildings_json(buildings_file)

        # Resources
        inventory_file = base_dir / 'inventory.json'
        if inventory_file.exists():
            all_mappings['resources'] = parse_inventory_json(inventory_file)

        # Research
        research_file = base_dir / 'research.html'
        if research_file.exists():
            all_mappings['research'] = parse_research_html(research_file)

        # Command Center Avatars
        cc_dir = base_dir / 'command_center'
        if cc_dir.exists():
            all_mappings['avatars'] = parse_command_center_html(cc_dir)

    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        return 1

    # Write output
    print(f"\nWriting mappings to: {output_file}")
    with open(output_file, 'w', encoding='utf-8') as f:
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
