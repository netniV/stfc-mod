import requests

apis_to_try = [
    "https://api.stfc.space/v1/ship",
    "https://staging-api.stfc.dev/v1/ship",
    "https://yuki.stfc.space/v1/ship",
]

for api_url in apis_to_try:
    print(f"\nTrying: {api_url}")
    try:
        r = requests.get(api_url, params={'n': 'web-main'}, timeout=10)
        print(f"  Status: {r.status_code}")
        if r.status_code == 200:
            data = r.json()
            print(f"  SUCCESS! Got {len(data)} items")
            print(f"  Sample: {data[0] if data else 'Empty'}")
            break
        else:
            print(f"  Response: {r.text[:100]}")
    except Exception as e:
        print(f"  Error: {e}")
