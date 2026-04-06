#!/usr/bin/env python3
"""
Fetch ship and trait data from Spock's Club API

Uses the official Spock's Club API endpoints to get accurate ship and trait mappings.
"""

import requests
import json
from pathlib import Path
import time

# API base URL
API_BASE = "https://api.spocks.club"
HEADERS = {
    'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) Gecko/20100101 Firefox/146.0',
    'Accept': 'application/json, text/javascript, */*; q=0.01',
    'Accept-Language': 'en-US,en;q=0.5',
    'Origin': 'https://next.spocks.club',
    'Referer': 'https://next.spocks.club/',
    'DNT': '1',
    'Sec-GPC': '1',
    'Connection': 'keep-alive',
    'Sec-Fetch-Dest': 'empty',
    'Sec-Fetch-Mode': 'cors',
    'Sec-Fetch-Site': 'same-site'
}

def fetch_api_data(endpoint):
    """Fetch data from a Spock's Club API endpoint"""
    url = f"{API_BASE}/{endpoint}"
    print(f"Fetching: {url}")

    try:
        response = requests.get(url, headers=HEADERS, timeout=10)
        response.raise_for_status()

        data = response.json()
        print(f"  Success! Got {len(data) if isinstance(data, (list, dict)) else 'unknown'} items")
        return data

    except Exception as e:
        print(f"  Error: {e}")
        return None

def main():
    print("Spock's Club API Data Fetcher")
    print("=" * 60)

    # Fetch all the data
    endpoints = {
        'ships': 'translations/en/ships',
        'traits': 'translations/en/traits',
        'officers': 'officer',
        'synergies': 'translations/en/synergies',
        'factions': 'translations/en/factions'
    }

    fetched_data = {}

    for name, endpoint in endpoints.items():
        print(f"\nFetching {name}...")
        data = fetch_api_data(endpoint)
        if data:
            fetched_data[name] = data
        time.sleep(0.5)  # Be nice to the API

    # Save raw data
    raw_output = Path('spocks_club_content_to_parse/api_data_raw.json')
    print(f"\nSaving raw data to {raw_output}...")
    raw_output.parent.mkdir(exist_ok=True)

    with open(raw_output, 'w', encoding='utf-8') as f:
        json.dump(fetched_data, f, indent=2, ensure_ascii=False)

    print(f"Raw data saved!")

    # Process ships data
    if 'ships' in fetched_data:
        ships_data = fetched_data['ships']
        print(f"\n{'=' * 60}")
        print(f"Ships Data Processing")
        print(f"{'=' * 60}")
        print(f"Total ships: {len(ships_data) if isinstance(ships_data, list) else 'unknown'}")

        # Show sample
        if isinstance(ships_data, list) and ships_data:
            print("\nSample ships:")
            for ship in ships_data[:5]:
                print(f"  {ship}")
        elif isinstance(ships_data, dict):
            print("\nSample ships:")
            for key, value in list(ships_data.items())[:5]:
                print(f"  {key}: {value}")

    # Process traits data
    if 'traits' in fetched_data:
        traits_data = fetched_data['traits']
        print(f"\n{'=' * 60}")
        print(f"Traits Data Processing")
        print(f"{'=' * 60}")
        print(f"Total traits: {len(traits_data) if isinstance(traits_data, (list, dict)) else 'unknown'}")

        # Show sample
        if isinstance(traits_data, list) and traits_data:
            print("\nSample traits:")
            for trait in traits_data[:5]:
                print(f"  {trait}")
        elif isinstance(traits_data, dict):
            print("\nSample traits:")
            for key, value in list(traits_data.items())[:5]:
                print(f"  {key}: {value}")

    print(f"\n{'=' * 60}")
    print("Next steps:")
    print("1. Review the raw data in api_data_raw.json")
    print("2. Run the conversion script to create proper mappings")
    print("3. Merge into stfc_id_mappings.json")

if __name__ == '__main__':
    main()
