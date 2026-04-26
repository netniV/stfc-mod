import requests
import json
from gist_config import GITHUB_TOKEN, GIST_ID

headers = {
    "Authorization": f"token {GITHUB_TOKEN}",
    "Accept": "application/vnd.github.v3+json"
}

# Fetch raw content directly to avoid API truncation
BASE_RAW = f"https://gist.githubusercontent.com/DrCord/{GIST_ID}/raw"

for filename in ["stfc_gamestate_full.json", "stfc_gamestate_delta.json"]:
    r = requests.get(f"{BASE_RAW}/{filename}", headers=headers)
    if r.status_code != 200:
        print(f"{filename}: HTTP {r.status_code}")
        continue
    content = r.json()
    print(f"\n=== {filename} ===")
    if filename == "stfc_gamestate_full.json":
        print(f"  export_type:   {content.get('export_type')}")
        print(f"  exported_at:   {content.get('meta', {}).get('exported_at')}")
        player = content.get("player", {})
        print(f"  player.name:   {player.get('name')}")
        print(f"  player.ops:    {player.get('ops_level')}")
        print(f"  player.power:  {player.get('power')}")
        print(f"  player.server: {player.get('server')}")
        print(f"  buildings:     {len(content.get('buildings', []))}")
        print(f"  research:      {len(content.get('research', []))}")
        print(f"  ships:         {len(content.get('ships', []))}")
        print(f"  officers:      {len(content.get('officers', []))}")
        print(f"  resources:     {len(content.get('resources', []))}")
        print(f"  faction_rep:   {len(content.get('faction_reputation', []))}")
        print(f"  blueprints:    {len(content.get('blueprints', []))}")
    else:
        print(f"  delta entries: {len(content)}")
        if content:
            print(f"  latest:        {content[-1].get('exported_at')}")
            print(f"  changes:       {content[-1].get('summary', {}).get('total_changes')}")

print("\n=== RAW URLs ===")
print(f"Full:  {BASE_RAW}/stfc_gamestate_full.json")
print(f"Delta: {BASE_RAW}/stfc_gamestate_delta.json")

