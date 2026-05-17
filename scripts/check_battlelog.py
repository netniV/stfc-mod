import requests
import json
from gist_config import GITHUB_TOKEN, GIST_ID

BASE_RAW = f"https://gist.githubusercontent.com/DrCord/{GIST_ID}/raw"
headers = {"Authorization": f"token {GITHUB_TOKEN}"}

r = requests.get(f"{BASE_RAW}/stfc_battlelog.json", headers=headers)
battles = r.json()
print(f"Total battles: {len(battles)}")
for b in battles:
    print(f"\n  id:       {b.get('id')}")
    print(f"  timestamp:{b.get('timestamp')}")
    print(f"  outcome:  {b.get('outcome')}")
    print(f"  location: {b.get('location')}")
    print(f"  ship:     {b.get('ship')}")
    combatants = b.get('combatants', [])
    print(f"  combatants ({len(combatants)}):")
    for c in combatants:
        print(f"    {c.get('Player Name')} ({c.get('Outcome')}) - {c.get('Ship Name')} lvl {c.get('Ship Level')}")
    rewards = b.get('rewards', [])
    reward_strs = [str(r.get('Reward Name')) + "x" + str(r.get('Count')) for r in rewards]
    print(f"  rewards: {reward_strs}")
    rounds = b.get('rounds', [])
    print(f"  rounds: {len(rounds)} events")
