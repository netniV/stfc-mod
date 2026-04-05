#!/usr/bin/env python3
"""
Extract Ship and Officer Data from STFC Gamestate Export

This script analyzes your actual gamestate export to extract:
1. Ship IDs you own (with their properties)
2. Officer IDs you own (with their traits)

We'll use this to reverse-engineer name mappings from your game data.
"""

import json
from pathlib import Path
from collections import defaultdict

# Path to your gamestate export
GAMESTATE_FILE = r"C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game\community_patch_gamestate.json"
OUTPUT_FILE = r"C:\Users\Cord42\Projects\stfc-community-mod\game_data_maps\reverse_engineered_data.json"

def extract_ship_data(gamestate):
    """Extract all ship IDs and properties from gamestate"""
    ships_data = {}

    if "ships" in gamestate and len(gamestate["ships"]) > 0:
        for ship in gamestate["ships"]:
            ship_id = ship.get("id")
            if ship_id:
                ships_data[str(ship_id)] = {
                    "id": ship_id,
                    "hull_id": ship.get("hull_id"),
                    "tier": ship.get("tier"),
                    "level": ship.get("level"),
                    "level_percentage": ship.get("level_percentage"),
                    "components": ship.get("components", []),
                    "name": f"Ship_{ship_id}",  # Placeholder - you'll need to identify manually
                    "source": "gamestate_export"
                }

    return ships_data

def extract_officer_data(gamestate):
    """Extract all officer IDs and properties from gamestate"""
    officers_data = {}

    if "officers" in gamestate and len(gamestate["officers"]) > 0:
        for officer in gamestate["officers"]:
            officer_id = officer.get("id")
            if officer_id:
                # Get existing mapped name if available
                existing_name = officer.get("name", f"Officer_{officer_id}")

                officers_data[str(officer_id)] = {
                    "id": officer_id,
                    "name": existing_name,
                    "rank": officer.get("rank"),
                    "level": officer.get("level"),
                    "shards": officer.get("shards"),
                    "faction": officer.get("faction"),
                    "rarity": officer.get("rarity"),
                    "synergy": officer.get("synergy"),
                    "traits": [],  # Will need to extract from game data
                    "source": "gamestate_export"
                }

    return officers_data

def extract_resource_summary(gamestate):
    """Get summary of resources to help identify ship/officer materials"""
    resource_summary = {
        "total_resources": 0,
        "ships_related": [],
        "officers_related": []
    }

    if "resources" in gamestate:
        resources = gamestate["resources"]
        resource_summary["total_resources"] = len(resources)

        for resource in resources:
            name = resource.get("name", "")
            res_id = resource.get("id")
            amount = resource.get("amount", 0)

            # Look for ship-related resources
            if any(keyword in name.lower() for keyword in ["ship", "hull", "blueprint", "bp_", "shard"]):
                if amount > 0:  # Only include if you have some
                    resource_summary["ships_related"].append({
                        "id": res_id,
                        "name": name,
                        "amount": amount
                    })

            # Look for officer-related resources
            if any(keyword in name.lower() for keyword in ["officer", "crew", "shard"]) and "ship" not in name.lower():
                if amount > 0:
                    resource_summary["officers_related"].append({
                        "id": res_id,
                        "name": name,
                        "amount": amount
                    })

    return resource_summary

def main():
    print("?? Extracting ship and officer data from gamestate export...")

    gamestate_path = Path(GAMESTATE_FILE)
    if not gamestate_path.exists():
        print(f"? Gamestate file not found: {GAMESTATE_FILE}")
        print("   Make sure the game is running and has exported data")
        return

    # Load gamestate
    with open(gamestate_path, 'r') as f:
        gamestate = json.load(f)

    print(f"? Loaded gamestate exported at: {gamestate.get('meta', {}).get('exported_at', 'unknown')}")

    # Extract data
    ships_data = extract_ship_data(gamestate)
    officers_data = extract_officer_data(gamestate)
    resource_summary = extract_resource_summary(gamestate)

    # Results
    result = {
        "extracted_at": gamestate.get("meta", {}).get("exported_at"),
        "ships": ships_data,
        "officers": officers_data,
        "resource_summary": resource_summary,
        "stats": {
            "total_ships": len(ships_data),
            "total_officers": len(officers_data),
            "ship_related_resources": len(resource_summary["ships_related"]),
            "officer_related_resources": len(resource_summary["officers_related"])
        }
    }

    # Save to file
    output_path = Path(OUTPUT_FILE)
    output_path.parent.mkdir(exist_ok=True)

    with open(output_path, 'w') as f:
        json.dump(result, f, indent=2)

    print(f"\n?? Extraction Results:")
    print(f"   Ships found: {result['stats']['total_ships']}")
    print(f"   Officers found: {result['stats']['total_officers']}")
    print(f"   Ship-related resources: {result['stats']['ship_related_resources']}")
    print(f"   Officer-related resources: {result['stats']['officer_related_resources']}")

    print(f"\n?? Saved to: {OUTPUT_FILE}")

    if result['stats']['total_ships'] > 0:
        print(f"\n?? Sample ships:")
        for ship_id, ship in list(ships_data.items())[:5]:
            print(f"   ID {ship_id}: Tier {ship.get('tier')}, Level {ship.get('level')}, Hull {ship.get('hull_id')}")

    if result['stats']['total_officers'] > 0:
        print(f"\n?? Sample officers:")
        for officer_id, officer in list(officers_data.items())[:5]:
            print(f"   ID {officer_id}: {officer.get('name')} - Rank {officer.get('rank')}, Level {officer.get('level')}")

    print("\n? Next steps:")
    print("   1. Review reverse_engineered_data.json")
    print("   2. Manually identify ship names from hull_id and tier")
    print("   3. Add officer trait data if available")
    print("   4. Merge into stfc_id_mappings.json")

if __name__ == "__main__":
    main()
