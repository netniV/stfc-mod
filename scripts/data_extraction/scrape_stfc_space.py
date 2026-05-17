#!/usr/bin/env python3
"""
STFC.space Ships Data Scraper

Scrapes ship IDs and names from stfc.space to build ship mappings.
"""

import requests
import json
import time
from pathlib import Path
from bs4 import BeautifulSoup
import re

# Output file
OUTPUT_FILE = Path(__file__).parent.parent.parent / "game_data_maps" / "ships_from_stfc_space.json"

def get_ships_list():
    """Get the list of all ships from stfc.space"""
    url = "https://stfc.space/ships"

    print(f"?? Fetching ships list from {url}")
    response = requests.get(url)

    if response.status_code != 200:
        print(f"? Failed to fetch ships list: HTTP {response.status_code}")
        return []

    soup = BeautifulSoup(response.text, 'html.parser')

    # Find all ship links - they should be in format /ships/{id}
    ship_links = []
    for link in soup.find_all('a', href=True):
        href = link['href']
        if href.startswith('/ships/') and '?' not in href:
            # Extract ship ID from URL
            match = re.match(r'/ships/(\d+)', href)
            if match:
                ship_id = match.group(1)
                ship_name = link.get_text(strip=True)
                if ship_name and ship_id:
                    ship_links.append({
                        'id': int(ship_id),
                        'name': ship_name,
                        'url': f"https://stfc.space{href}"
                    })

    # Remove duplicates (same ship might appear multiple times)
    seen = set()
    unique_ships = []
    for ship in ship_links:
        if ship['id'] not in seen:
            seen.add(ship['id'])
            unique_ships.append(ship)

    print(f"? Found {len(unique_ships)} unique ships")
    return unique_ships

def get_ship_details(ship_id):
    """Get detailed information about a specific ship"""
    url = f"https://stfc.space/ships/{ship_id}?tier=1"

    try:
        response = requests.get(url)
        if response.status_code != 200:
            print(f"??  Failed to fetch ship {ship_id}: HTTP {response.status_code}")
            return None

        soup = BeautifulSoup(response.text, 'html.parser')

        # Try to extract ship metadata from the page
        # This is a simplified version - may need adjustment based on actual page structure
        ship_data = {
            'id': ship_id,
            'name': None,
            'type': None,
            'faction': None,
            'rarity': None
        }

        # Find the ship name (usually in h1 or title)
        title = soup.find('h1')
        if title:
            ship_data['name'] = title.get_text(strip=True)
        else:
            # Fallback to page title
            title_tag = soup.find('title')
            if title_tag:
                # Usually "Ship Name - STFC.space"
                ship_data['name'] = title_tag.get_text(strip=True).split(' - ')[0]

        # Try to find ship type (Interceptor, Explorer, Battleship, Survey)
        # This might be in various places, check common patterns
        for text in soup.stripped_strings:
            if 'Interceptor' in text:
                ship_data['type'] = 'Interceptor'
                break
            elif 'Explorer' in text:
                ship_data['type'] = 'Explorer'
                break
            elif 'Battleship' in text:
                ship_data['type'] = 'Battleship'
                break
            elif 'Survey' in text:
                ship_data['type'] = 'Survey'
                break

        return ship_data

    except Exception as e:
        print(f"? Error fetching ship {ship_id}: {e}")
        return None

def scrape_all_ships():
    """Scrape all ships from stfc.space"""
    ships = get_ships_list()

    if not ships:
        print("? No ships found")
        return {}

    ships_data = {}

    print(f"\n?? Fetching details for {len(ships)} ships...")
    for i, ship in enumerate(ships, 1):
        ship_id = ship['id']
        ship_name = ship['name']

        print(f"[{i}/{len(ships)}] {ship_name} (ID: {ship_id})")

        # Use the basic data from the list
        ships_data[str(ship_id)] = {
            'id': ship_id,
            'name': ship_name,
            'hull_id': ship_id,  # Assuming hull_id matches the ship page ID
            'source': 'stfc.space'
        }

        # Rate limiting - be nice to the server
        time.sleep(0.5)

    return ships_data

def main():
    print("?? STFC.space Ships Scraper")
    print("=" * 60)

    ships_data = scrape_all_ships()

    if not ships_data:
        print("\n? No ships data scraped")
        return

    # Save to file
    OUTPUT_FILE.parent.mkdir(exist_ok=True)

    with open(OUTPUT_FILE, 'w') as f:
        json.dump(ships_data, f, indent=2)

    print(f"\n? Saved {len(ships_data)} ships to {OUTPUT_FILE}")

    # Show sample
    print("\n?? Sample ships:")
    for ship_id, ship in list(ships_data.items())[:5]:
        print(f"   {ship['name']} (ID: {ship_id})")

    print("\n?? Next steps:")
    print("   1. Review ships_from_stfc_space.json")
    print("   2. Merge with existing stfc_id_mappings.json")
    print("   3. Test exports with ship names enriched")

if __name__ == "__main__":
    main()
