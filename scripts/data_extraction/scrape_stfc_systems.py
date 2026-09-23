#!/usr/bin/env python3
"""
Scrape system IDs and names from stfc.space/systems pages.
Outputs: community_patch/game_data_maps/stfc_systems.json
"""

from selenium import webdriver
from selenium.webdriver.chrome.options import Options
from selenium.webdriver.common.by import By
import json
import time
from pathlib import Path

TOTAL_PAGES = 123
OUTPUT_FILE = Path(__file__).parent.parent.parent / "community_patch" / "game_data_maps" / "stfc_systems.json"


def make_driver():
    opts = Options()
    opts.add_argument("--headless")
    opts.add_argument("--no-sandbox")
    return webdriver.Chrome(options=opts)


def scrape_page(driver, page_num):
    url = f"https://stfc.space/systems?f=$name=%26page:{page_num}"
    driver.get(url)
    time.sleep(3)  # wait for JS render
    body_text = driver.find_element(By.TAG_NAME, "body").text
    return body_text


def scrape_systems_page(driver, page_num):
    """Return list of (system_id, system_name) from one page."""
    url = f"https://stfc.space/systems?f=$name=%26page:{page_num}"
    driver.get(url)
    time.sleep(3)
    links = driver.find_elements(By.CSS_SELECTOR, "a[href*='/systems/']")
    results = []
    for link in links:
        href = link.get_attribute("href") or ""
        parts = href.rstrip("/").split("/")
        if not parts[-1].isdigit():
            continue
        system_id = int(parts[-1])
        # Name is the first line of the link text
        raw_text = link.text.strip()
        name = raw_text.splitlines()[0].strip() if raw_text else ""
        if name:
            results.append((system_id, name))
    return results


def main():
    driver = make_driver()
    systems = {}  # id -> name
    try:
        for page in range(1, TOTAL_PAGES + 1):
            entries = scrape_systems_page(driver, page)
            for sid, name in entries:
                systems[sid] = name
            print(f"Page {page}/{TOTAL_PAGES}: {len(entries)} systems (total so far: {len(systems)})")
    finally:
        driver.quit()

    print(f"\nTotal systems scraped: {len(systems)}")

    # Write output
    OUTPUT_FILE.parent.mkdir(parents=True, exist_ok=True)
    with open(OUTPUT_FILE, "w", encoding="utf-8") as f:
        json.dump(systems, f, ensure_ascii=False, indent=2, sort_keys=True)
    print(f"Written to {OUTPUT_FILE}")

    # Verify known systems from user's ships
    known = {
        "Innlasn Alpha": None,
        "Ferraria": None,
        "Zhang": None,
        "Nikola": None,
    }
    for sid, name in systems.items():
        for k in known:
            if k.lower() in name.lower():
                known[k] = sid
    print("\nVerification (known ship locations):")
    for name, sid in known.items():
        print(f"  {name}: {sid}")


if __name__ == "__main__":
    main()
