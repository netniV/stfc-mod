#!/usr/bin/env python3
"""Check player.json export for system name enrichment."""
import json

PLAYER_JSON = "C:/Games/Star Trek Fleet Command/Star Trek Fleet Command/default/game/community_patch/game_state_exports/player.json"

with open(PLAYER_JSON, encoding="utf-8") as f:
    d = json.load(f)

player = d.get("player", {})
station = d.get("station", {})
print("Station:")
for k, v in station.items():
    print(f"  {k}: {v}")

print("\nDrydock:")
for e in d.get("drydocks", []):
    sid = e.get("system_id", "?")
    sname = e.get("system_name", "MISSING")
    at_home = e.get("is_at_home_station", "MISSING")
    print(f"  drydock {e['drydock_id']}: ship={e['ship_id']} system_id={sid} system_name={sname} status={e.get('status','?')} at_home={at_home}")

alliance = player.get("alliance", {})
if isinstance(alliance, dict):
    print(f"\nAlliance starbase: system_id={alliance.get('starbase_system_id','?')} system_name={alliance.get('starbase_system_name','MISSING')}")
else:
    print(f"\nAlliance: {alliance}")
