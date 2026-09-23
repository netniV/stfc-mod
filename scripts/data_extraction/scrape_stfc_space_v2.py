#!/usr/bin/env python3
"""
STFC.space Ships Data Scraper - API Approach

Since stfc.space is a JS app, we'll try to find and use their API directly.
Alternatively, we can scrape individual ship pages if we know the IDs.
"""

import requests
import json
import time
from pathlib import Path

OUTPUT_FILE = Path(__file__).parent.parent.parent / "game_data_maps" / "ships_from_stfc_space.json"

# Try to find the API endpoint
def find_api_endpoint():
    """Try to discover the API endpoint used by stfc.space"""

    # Common API patterns for ship data sites
    possible_endpoints = [
        "https://stfc.space/api/ships",
        "https://api.stfc.space/ships",
        "https://stfc.space/api/v1/ships",
        "https://stfc.space/data/ships.json",
    ]

    for endpoint in possible_endpoints:
        print(f"?? Trying {endpoint}...")
        try:
            response = requests.get(endpoint, timeout=5)
            if response.status_code == 200:
                try:
                    data = response.json()
                    print(f"? Found API endpoint: {endpoint}")
                    print(f"   Response type: {type(data)}")
                    if isinstance(data, list):
                        print(f"   Items: {len(data)}")
                    elif isinstance(data, dict):
                        print(f"   Keys: {list(data.keys())[:5]}")
                    return endpoint, data
                except:
                    pass
        except:
            pass

    return None, None

def scrape_with_known_ids():
    """
    Since we can't easily get the full list, let's use the IDs from the
    reverse-engineered data (hull_ids from your gamestate) and check if
    stfc.space has those ships.
    """

    # Load your extracted ships data
    reverse_eng_file = Path(__file__).parent.parent.parent / "game_data_maps" / "reverse_engineered_data.json"

    if not reverse_eng_file.exists():
        print("? reverse_engineered_data.json not found")
        return {}

    with open(reverse_eng_file, 'r') as f:
        data = json.load(f)

    ships = data.get('ships', {})
    if not ships:
        print("? No ships in reverse_engineered_data.json")
        return {}

    print(f"\n?? Found {len(ships)} ships in your gamestate")
    print("?? Checking stfc.space for each hull_id...")

    ships_data = {}
    found_count = 0

    for ship_id, ship in ships.items():
        hull_id = ship.get('hull_id')
        if not hull_id:
            continue

        # Try to fetch this ship from stfc.space
        url = f"https://stfc.space/ships/{hull_id}"

        try:
            response = requests.get(url, timeout=5)

            if response.status_code == 200:
                # Page exists! Try to parse it
                from bs4 import BeautifulSoup
                soup = BeautifulSoup(response.text, 'html.parser')

                # Try to find ship name in the page
                # Since it's JS-rendered, we might not get much, but try anyway
                title = soup.find('title')
                if title:
                    page_title = title.get_text(strip=True)
                    # Usually "Ship Name - STFC Database" or similar
                    ship_name = page_title.split(' - ')[0].strip()

                    if ship_name and ship_name != "STFC Database":
                        ships_data[str(hull_id)] = {
                            'id': hull_id,
                            'name': ship_name,
                            'hull_id': hull_id,
                            'tier': ship.get('tier'),
                            'source': 'stfc.space',
                            'url': url
                        }
                        found_count += 1
                        print(f"? {ship_name} (hull_id: {hull_id}, tier: {ship.get('tier')})")
                    else:
                        print(f"??  Page exists for {hull_id} but couldn't extract name")
                else:
                    print(f"??  Page exists for {hull_id} but no title found")
            else:
                print(f"? hull_id {hull_id}: HTTP {response.status_code}")

        except Exception as e:
            print(f"? Error checking hull_id {hull_id}: {e}")

        # Rate limiting
        time.sleep(0.3)

    print(f"\n? Successfully identified {found_count} ships from stfc.space")
    return ships_data

def main():
    print("?? STFC.space Ships Scraper v2")
    print("=" * 60)

    # Try to find API first
    print("\n1?? Attempting to find API endpoint...")
    api_endpoint, api_data = find_api_endpoint()

    if api_endpoint and api_data:
        print(f"\n? Using API endpoint: {api_endpoint}")
        # Process API data here
        ships_data = {}
        # TODO: Parse API data structure when we find it
    else:
        print("\n??  No API endpoint found")
        print("\n2?? Falling back to scraping individual ship pages...")
        ships_data = scrape_with_known_ids()

    if not ships_data:
        print("\n? No ships data collected")
        print("\n?? Alternative: stfc.space appears to be a JavaScript app.")
        print("   You may need to:")
        print("   1. Use browser DevTools to find the actual API endpoint")
        print("   2. Or manually build a ships list from known URLs")
        return

    # Save results
    OUTPUT_FILE.parent.mkdir(exist_ok=True)
    with open(OUTPUT_FILE, 'w') as f:
        json.dump(ships_data, f, indent=2)

    print(f"\n? Saved {len(ships_data)} ships to {OUTPUT_FILE}")

    # Sample
    if ships_data:
        print("\n?? Sample ships:")
        for hull_id, ship in list(ships_data.items())[:5]:
            print(f"   {ship['name']} (hull_id: {hull_id}, tier: {ship.get('tier')})")

if __name__ == "__main__":
    main()
