#!/usr/bin/env python3
"""
Verify ship names in gamestate export
"""

import json
from pathlib import Path

export_file = Path('C:/Games/Star Trek Fleet Command/Star Trek Fleet Command/default/game/community_patch_gamestate.json')

with open(export_file, 'r') as f:
    data = json.load(f)

ships = data.get('ships', [])
officers = data.get('officers', [])

print(f"Total ships: {len(ships)}")
print(f"Total officers: {len(officers)}")

print("\n" + "="*60)
print("SHIPS WITH NAMES:")
print("="*60)
for ship in ships[:10]:
    name = ship.get('name', 'NO NAME')
    hull_id = ship.get('hull_id')
    tier = ship.get('tier')
    level = ship.get('level')
    print(f"{name:20} | Hull: {hull_id:10} | Tier: {tier} | Level: {level}")

if len(ships) > 10:
    print(f"... and {len(ships) - 10} more ships")

print("\n" + "="*60)
print("OFFICERS WITH TRAITS:")
print("="*60)
officers_with_traits = [o for o in officers if 'traits' in o]
print(f"Officers with trait data: {len(officers_with_traits)}/{len(officers)}")

for officer in officers_with_traits[:5]:
    name = officer.get('name', 'NO NAME')
    traits = officer.get('traits', [])
    trait_names = [t.get('name', '?') for t in traits]
    print(f"{name:20} | Traits: {', '.join(trait_names)}")

print("\n" + "="*60)
print("SUCCESS! ?")
print("="*60)
print(f"? {len([s for s in ships if 'name' in s])} ships have names")
print(f"? {len(officers_with_traits)} officers have trait data")
