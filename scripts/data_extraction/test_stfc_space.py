import requests
from bs4 import BeautifulSoup

r = requests.get('https://stfc.space/ships')
soup = BeautifulSoup(r.text, 'html.parser')

print(f"Status: {r.status_code}")
print(f"Title: {soup.find('title').get_text() if soup.find('title') else 'No title'}")
print(f"\nTotal links: {len(soup.find_all('a'))}")
print("\nFirst 20 links:")
for a in soup.find_all('a', href=True)[:20]:
    print(f"  {a.get('href')}: {a.get_text(strip=True)[:50]}")
