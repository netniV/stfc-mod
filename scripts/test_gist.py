import requests
from gist_config import GITHUB_TOKEN, GIST_ID

headers = {
    "Authorization": f"token {GITHUB_TOKEN}",
    "Accept": "application/vnd.github.v3+json"
}

r = requests.get(f"https://api.github.com/gists/{GIST_ID}", headers=headers)
print(f"Status: {r.status_code}")
if r.status_code == 200:
    data = r.json()
    print(f"Description: {data.get('description')}")
    print(f"Files: {list(data['files'].keys())}")
    print(f"Owner: {data['owner']['login']}")
else:
    print(r.text)
