import json

GAME_DIR = r"C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game"
data = json.load(open(f"{GAME_DIR}/community_patch_gamestate.json"))
player = data.get('player', {})
faction_rep = data.get('faction_reputation', [])
blueprints = data.get('blueprints', [])

print('PLAYER:', json.dumps(player, indent=2))
print(f'\nFACTION REP: {len(faction_rep)} entries')
for r in faction_rep[:5]:
    print(' ', r)
print(f'\nBLUEPRINTS: {len(blueprints)} entries')
for b in blueprints[:5]:
    print(' ', b)

officers = data.get('officers', [])
officers_with_traits = [o for o in officers if o.get('traits')]
print(f'\nOFFICERS WITH TRAITS: {len(officers_with_traits)}/{len(officers)}')
if officers_with_traits:
    o = officers_with_traits[0]
    print(f"  Sample: {o['name']} -> {[t.get('name') + ' lvl ' + str(t.get('ability_level', '?')) for t in o['traits']]}")
