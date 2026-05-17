#!/usr/bin/env python3
"""
Spock's Club ID Mapping Extractor

Scrapes the Spock's Club website to extract ID mappings for:
- Research projects
- Buildings
- Ships
- Officers
- Resources

Usage:
    python scrape_spocks_club.py --output mappings.json

Requirements:
    pip install requests beautifulsoup4 lxml
"""

import argparse
import json
import re
import requests
from bs4 import BeautifulSoup
from typing import Dict, List, Any, Optional
import time
import sys

class SpocksClubScraper:
    """Scraper for extracting ID mappings from Spock's Club website"""

    BASE_URL = "https://spocks.club"

    def __init__(self, delay: float = 1.0):
        """
        Initialize scraper

        Args:
            delay: Delay between requests in seconds (be nice to their server)
        """
        self.delay = delay
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': 'STFC-Community-Mod ID Mapper (Educational/Non-Commercial)'
        })
        self.mappings = {
            'research': {},
            'buildings': {},
            'ships': {},
            'officers': {},
            'resources': {},
            'components': {}
        }

    def scrape_all(self) -> Dict[str, Dict[int, Any]]:
        """Scrape all ID mappings"""
        print("Starting Spock's Club ID mapping extraction...")
        print(f"Base URL: {self.BASE_URL}\n")

        # Scrape each category
        self.scrape_officers()
        time.sleep(self.delay)

        self.scrape_research()
        time.sleep(self.delay)

        self.scrape_buildings()
        time.sleep(self.delay)

        self.scrape_ships()
        time.sleep(self.delay)

        self.scrape_resources()
        time.sleep(self.delay)

        return self.mappings

    def _fetch_page(self, url: str) -> Optional[BeautifulSoup]:
        """Fetch and parse a page"""
        try:
            print(f"  Fetching: {url}")
            response = self.session.get(url, timeout=30)
            response.raise_for_status()
            return BeautifulSoup(response.content, 'lxml')
        except Exception as e:
            print(f"  ERROR fetching {url}: {e}")
            return None

    def scrape_officers(self):
        """Scrape officer IDs and names from officer cards"""
        print("Scraping officers...")

        # Official URL for officers page
        urls_to_try = [
            f"{self.BASE_URL}/officers/",
            f"{self.BASE_URL}/officers"
        ]

        soup = None
        for url in urls_to_try:
            soup = self._fetch_page(url)
            if soup:
                # Check if we found officer cards
                officer_cards = soup.find_all('div', {'data-officer-id': True})
                if officer_cards:
                    print(f"  Found {len(officer_cards)} officers at {url}")
                    break

        if not soup:
            print("  Could not find officers page")
            return

        # Find all officer cards with data-officer-id attribute
        officer_cards = soup.find_all('div', {'data-officer-id': True})

        for card in officer_cards:
            officer_id = card.get('data-officer-id')
            if not officer_id:
                continue

            try:
                officer_id = int(officer_id)
            except ValueError:
                continue

            # Extract officer name from card body
            card_body = card.find('div', class_='card-body')
            name = card_body.get_text(strip=True) if card_body else f"Officer {officer_id}"

            # Extract additional details
            details_div = card.find('div', class_='officercarddetails')
            faction = None
            ability = None
            if details_div:
                details_text = details_div.get_text(strip=True)
                lines = [line.strip() for line in details_text.split('\n') if line.strip()]
                if len(lines) >= 1:
                    faction = lines[0]
                if len(lines) >= 2:
                    ability = lines[1]

            # Get rarity from class
            rarity = None
            classes = card.get('class', [])
            for cls in classes:
                if 'officer' in cls.lower() and cls != 'officer' and cls != 'officercard':
                    rarity = cls.replace('officer', '').replace('Uncommon', 'Uncommon').replace('Rare', 'Rare').replace('Epic', 'Epic')
                    break

            self.mappings['officers'][officer_id] = {
                'name': name.strip(),
                'faction': faction,
                'ability': ability,
                'rarity': rarity
            }

        print(f"  Extracted {len(self.mappings['officers'])} officers")

    def scrape_research(self):
        """Scrape research project IDs and names"""
        print("Scraping research projects...")

        urls_to_try = [
            f"{self.BASE_URL}/research/",
            f"{self.BASE_URL}/research"
        ]

        soup = None
        for url in urls_to_try:
            soup = self._fetch_page(url)
            if soup:
                # Check for research cards with data-research-id or similar
                research_items = soup.find_all('div', {'data-research-id': True})
                if not research_items:
                    research_items = soup.find_all('div', {'data-tech-id': True})
                if research_items:
                    print(f"  Found {len(research_items)} research items at {url}")
                    break

        if not soup:
            print("  Could not find research page - may need manual URL")
            return

        # Try different attribute patterns
        for attr_name in ['data-research-id', 'data-tech-id', 'data-id']:
            research_items = soup.find_all('div', {attr_name: True})

            for item in research_items:
                research_id = item.get(attr_name)
                if not research_id:
                    continue

                try:
                    research_id = int(research_id)
                except ValueError:
                    continue

                # Extract name
                name_elem = item.find('div', class_='card-body')
                if not name_elem:
                    name_elem = item.find(['h5', 'h6', 'span', 'div'])

                name = name_elem.get_text(strip=True) if name_elem else f"Research {research_id}"

                self.mappings['research'][research_id] = {
                    'name': name.strip()
                }

        print(f"  Extracted {len(self.mappings['research'])} research projects")

    def scrape_buildings(self):
        """Scrape building IDs and names"""
        print("Scraping buildings...")

        urls_to_try = [
            f"{self.BASE_URL}/buildings/",
            f"{self.BASE_URL}/buildings",
            f"{self.BASE_URL}/commandcenter/",
            f"{self.BASE_URL}/commandcenter"
        ]

        soup = None
        for url in urls_to_try:
            soup = self._fetch_page(url)
            if soup:
                building_items = soup.find_all('div', {'data-building-id': True})
                if not building_items:
                    building_items = soup.find_all('div', {'data-module-id': True})
                if building_items:
                    print(f"  Found {len(building_items)} buildings at {url}")
                    break

        if not soup:
            print("  Could not find buildings page - may need manual URL")
            return

        for attr_name in ['data-building-id', 'data-module-id', 'data-id']:
            building_items = soup.find_all('div', {attr_name: True})

            for item in building_items:
                building_id = item.get(attr_name)
                if not building_id:
                    continue

                try:
                    building_id = int(building_id)
                except ValueError:
                    continue

                name_elem = item.find('div', class_='card-body')
                if not name_elem:
                    name_elem = item.find(['h5', 'h6', 'span', 'div'])

                name = name_elem.get_text(strip=True) if name_elem else f"Building {building_id}"

                self.mappings['buildings'][building_id] = {
                    'name': name.strip()
                }

        print(f"  Extracted {len(self.mappings['buildings'])} buildings")

    def scrape_ships(self):
        """Scrape ship hull IDs and names"""
        print("Scraping ships...")

        urls_to_try = [
            f"{self.BASE_URL}/ships",
            f"{self.BASE_URL}/fleet"
        ]

        soup = None
        for url in urls_to_try:
            soup = self._fetch_page(url)
            if soup:
                ship_items = soup.find_all('div', {'data-ship-id': True})
                if not ship_items:
                    ship_items = soup.find_all('div', {'data-hull-id': True})
                if ship_items:
                    print(f"  Found {len(ship_items)} ships at {url}")
                    break

        if not soup:
            print("  Could not find ships page - may need manual URL")
            return

        for attr_name in ['data-ship-id', 'data-hull-id', 'data-id']:
            ship_items = soup.find_all('div', {attr_name: True})

            for item in ship_items:
                ship_id = item.get(attr_name)
                if not ship_id:
                    continue

                try:
                    ship_id = int(ship_id)
                except ValueError:
                    continue

                name_elem = item.find('div', class_='card-body')
                if not name_elem:
                    name_elem = item.find(['h5', 'h6', 'span', 'div'])

                name = name_elem.get_text(strip=True) if name_elem else f"Ship {ship_id}"

                self.mappings['ships'][ship_id] = {
                    'name': name.strip()
                }

        print(f"  Extracted {len(self.mappings['ships'])} ships")

    def scrape_resources(self):
        """Scrape resource IDs and names"""
        print("Scraping resources...")

        urls_to_try = [
            f"{self.BASE_URL}/resources",
            f"{self.BASE_URL}/inventory"
        ]

        soup = None
        for url in urls_to_try:
            soup = self._fetch_page(url)
            if soup:
                resource_items = soup.find_all('div', {'data-resource-id': True})
                if resource_items:
                    print(f"  Found {len(resource_items)} resources at {url}")
                    break

        if not soup:
            print("  Could not find resources page - may need manual URL")
            return

        resource_items = soup.find_all('div', {'data-resource-id': True})

        for item in resource_items:
            resource_id = item.get('data-resource-id')
            if not resource_id:
                continue

            try:
                resource_id = int(resource_id)
            except ValueError:
                continue

            name_elem = item.find('div', class_='card-body')
            if not name_elem:
                name_elem = item.find(['h5', 'h6', 'span', 'div'])

            name = name_elem.get_text(strip=True) if name_elem else f"Resource {resource_id}"

            self.mappings['resources'][resource_id] = {
                'name': name.strip()
            }

        print(f"  Extracted {len(self.mappings['resources'])} resources")

    def save_mappings(self, output_file: str):
        """Save mappings to JSON file"""
        # Convert integer keys to strings for JSON compatibility
        output_data = {}
        for category, items in self.mappings.items():
            output_data[category] = {str(k): v for k, v in items.items()}

        with open(output_file, 'w', encoding='utf-8') as f:
            json.dump(output_data, f, indent=2, ensure_ascii=False)

        print(f"\n{'='*60}")
        print(f"Mappings saved to: {output_file}")
        print(f"{'='*60}")
        print(f"  Research:   {len(self.mappings['research']):>5} entries")
        print(f"  Buildings:  {len(self.mappings['buildings']):>5} entries")
        print(f"  Ships:      {len(self.mappings['ships']):>5} entries")
        print(f"  Officers:   {len(self.mappings['officers']):>5} entries")
        print(f"  Resources:  {len(self.mappings['resources']):>5} entries")
        print(f"  Components: {len(self.mappings['components']):>5} entries")
        print(f"{'='*60}")


def main():
    parser = argparse.ArgumentParser(
        description='Extract ID mappings from Spock\'s Club website',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  python scrape_spocks_club.py
  python scrape_spocks_club.py --output my_mappings.json --delay 2.0

Note: This scraper is designed for educational purposes to extract
publicly available data for use with the STFC Community Mod.
        '''
    )
    parser.add_argument(
        '--output', '-o',
        default='stfc_id_mappings.json',
        help='Output JSON file (default: stfc_id_mappings.json)'
    )
    parser.add_argument(
        '--delay', '-d',
        type=float,
        default=1.0,
        help='Delay between requests in seconds (default: 1.0)'
    )
    parser.add_argument(
        '--url',
        default='https://spocks.club',
        help='Base URL for Spock\'s Club (default: https://spocks.club)'
    )

    args = parser.parse_args()

    try:
        scraper = SpocksClubScraper(delay=args.delay)
        scraper.BASE_URL = args.url
        scraper.scrape_all()
        scraper.save_mappings(args.output)
        return 0
    except KeyboardInterrupt:
        print("\n\nScraping interrupted by user")
        return 1
    except Exception as e:
        print(f"\nERROR: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        return 1


if __name__ == '__main__':
    sys.exit(main())
