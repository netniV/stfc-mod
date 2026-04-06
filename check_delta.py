import json

deltas = json.load(open('community_patch_gamestate_delta.json'))
print(f'Deltas: {len(deltas)}')
if deltas:
    latest = deltas[-1]
    print(f'Latest delta keys: {list(latest.keys())}')
    if 'player' in latest:
        print(f'Player in delta: {json.dumps(latest["player"], indent=2)}')
    changes = latest.get('changes', {})
    print(f'Change categories: {list(changes.keys())}')
    if 'player' in changes:
        print(f'Player changes: {changes["player"]}')
