#!/usr/bin/env python3
"""
Check what data we have and what's missing for player, faction rep, and blueprints
"""

import json
from pathlib import Path

export_file = Path('C:/Games/Star Trek Fleet Command/Star Trek Fleet Command/default/game/community_patch_gamestate.json')

with open(export_file, 'r') as f:
    data = json.load(f)

print("="*60)
print("CURRENT GAMESTATE DATA ANALYSIS")
print("="*60)

# Check player data
player = data.get('player', {})
print("\nPLAYER DATA:")
print(f"  Name: {player.get('name', 'MISSING')}")
print(f"  Alliance: {player.get('alliance', 'MISSING')}")
print(f"  Ops Level: {player.get('ops_level', 'MISSING')}")
print(f"  Power: {player.get('power', 'MISSING')}")

# Check faction reputation
faction_rep = data.get('faction_reputation', [])
print(f"\nFACTION REPUTATION: {len(faction_rep)} entries")
if faction_rep:
    for rep in faction_rep[:5]:
        print(f"  {rep}")
else:
    print("  Currently empty placeholder")

# Check blueprints
blueprints = data.get('blueprints', [])
print(f"\nBLUEPRINTS: {len(blueprints)} entries")
if blueprints:
    for bp in blueprints[:5]:
        print(f"  {bp}")
else:
    print("  Currently empty placeholder")

# Look for faction points in resources
resources = data.get('resources', [])
print(f"\nRESOURCES: {len(resources)} total")

faction_keywords = ['federation', 'klingon', 'romulan', 'faction', 'rep']
faction_resources = [r for r in resources if any(kw in r.get('name', '').lower() for kw in faction_keywords)]
print(f"Faction-related resources found: {len(faction_resources)}")
for r in faction_resources[:10]:
    print(f"  {r.get('name', '?'):40} | id: {r.get('id')} | amount: {r.get('amount')}")

# Look for blueprint resources
bp_keywords = ['blueprint', 'blueprints', 'bp_']
bp_resources = [r for r in resources if any(kw in r.get('name', '').lower() for kw in bp_keywords)]
print(f"\nBlueprint-related resources found: {len(bp_resources)}")
for r in bp_resources[:10]:
    print(f"  {r.get('name', '?'):40} | id: {r.get('id')} | amount: {r.get('amount')}")

# Show all top-level keys in export
print("\n" + "="*60)
print("ALL TOP-LEVEL KEYS IN EXPORT:")
for key in data.keys():
    val = data[key]
    count = len(val) if isinstance(val, (list, dict)) else val
    print(f"  {key}: {count}")
