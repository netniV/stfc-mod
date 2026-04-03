#!/usr/bin/env python3
"""
STFC Game State to GitHub Gist Sync Script

Automatically syncs your STFC community mod gamestate JSON to a GitHub Gist
for easy AI access.

Setup:
1. Create a GitHub Personal Access Token with 'gist' scope:
   https://github.com/settings/tokens

2. Create a new Gist (can be secret):
   https://gist.github.com/

3. Edit the CONFIG section below with your details

4. Run: python sync_to_gist.py
"""

import requests
import time
import json
import hashlib
from pathlib import Path
from datetime import datetime

# ============================================================================
# CONFIG - EDIT THESE VALUES
# ============================================================================

# Your GitHub Personal Access Token (with 'gist' scope)
GITHUB_TOKEN = "ghp_YOUR_TOKEN_HERE"

# Your Gist ID (from the URL: https://gist.github.com/username/{THIS_PART})
GIST_ID = "YOUR_GIST_ID_HERE"

# Path to your STFC game state JSON file
GAMESTATE_FILE = r"C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game\community_patch_gamestate.json"

# How often to check for changes (seconds)
CHECK_INTERVAL = 60

# Filename in the Gist
GIST_FILENAME = "stfc_gamestate.json"

# ============================================================================
# NO NEED TO EDIT BELOW THIS LINE
# ============================================================================

class GameStateSync:
    def __init__(self):
        self.last_hash = None
        self.gamestate_path = Path(GAMESTATE_FILE)
        self.api_url = f"https://api.github.com/gists/{GIST_ID}"
        self.headers = {
            "Authorization": f"token {GITHUB_TOKEN}",
            "Accept": "application/vnd.github.v3+json"
        }

    def calculate_file_hash(self):
        """Calculate MD5 hash of file to detect changes"""
        if not self.gamestate_path.exists():
            return None

        with open(self.gamestate_path, 'rb') as f:
            return hashlib.md5(f.read()).hexdigest()

    def validate_json(self):
        """Validate that the file contains valid JSON"""
        try:
            with open(self.gamestate_path, 'r') as f:
                json.load(f)
            return True
        except json.JSONDecodeError as e:
            print(f"??  Invalid JSON in gamestate file: {e}")
            return False

    def update_gist(self):
        """Upload the current gamestate to Gist"""
        try:
            with open(self.gamestate_path, 'r') as f:
                content = f.read()

            # Parse to get exported_at timestamp for logging
            try:
                data = json.loads(content)
                exported_at = data.get('exported_at', 'unknown')
            except:
                exported_at = 'unknown'

            payload = {
                "files": {
                    GIST_FILENAME: {
                        "content": content
                    }
                }
            }

            response = requests.patch(self.api_url, headers=self.headers, json=payload)

            if response.status_code == 200:
                now = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
                print(f"? [{now}] Synced gamestate (exported at {exported_at})")
                return True
            else:
                print(f"? Failed to update Gist: HTTP {response.status_code}")
                print(f"   Response: {response.text}")
                return False

        except FileNotFoundError:
            print(f"??  Gamestate file not found: {self.gamestate_path}")
            return False
        except requests.RequestException as e:
            print(f"? Network error: {e}")
            return False
        except Exception as e:
            print(f"? Unexpected error: {e}")
            return False

    def get_gist_url(self):
        """Get the raw URL for the Gist"""
        try:
            response = requests.get(self.api_url, headers=self.headers)
            if response.status_code == 200:
                data = response.json()
                return data['files'][GIST_FILENAME]['raw_url']
        except:
            pass
        return f"https://gist.githubusercontent.com/USERNAME/{GIST_ID}/raw/{GIST_FILENAME}"

    def run(self):
        """Main sync loop"""
        print("?? STFC Game State ? GitHub Gist Sync")
        print(f"?? Watching: {self.gamestate_path}")
        print(f"?? Gist ID: {GIST_ID}")
        print(f"??  Check interval: {CHECK_INTERVAL}s")
        print("\n" + "="*60)

        # Check if file exists
        if not self.gamestate_path.exists():
            print(f"\n??  WARNING: Gamestate file not found!")
            print(f"   Make sure STFC is running with the community mod installed")
            print(f"   and enabled = true in [gamestate_export] section\n")

        print("\n?? Tip: Share this URL with AI assistants:")
        print(f"   {self.get_gist_url()}\n")
        print("Press Ctrl+C to stop\n")

        try:
            while True:
                if self.gamestate_path.exists():
                    current_hash = self.calculate_file_hash()

                    # Only upload if file changed
                    if current_hash != self.last_hash:
                        if self.validate_json():
                            if self.update_gist():
                                self.last_hash = current_hash

                time.sleep(CHECK_INTERVAL)

        except KeyboardInterrupt:
            print("\n\n?? Sync stopped by user")
        except Exception as e:
            print(f"\n? Fatal error: {e}")

def main():
    # Validate configuration
    if GITHUB_TOKEN == "ghp_YOUR_TOKEN_HERE":
        print("? ERROR: Please edit the script and set your GITHUB_TOKEN")
        print("   Get a token from: https://github.com/settings/tokens")
        return

    if GIST_ID == "YOUR_GIST_ID_HERE":
        print("? ERROR: Please edit the script and set your GIST_ID")
        print("   Create a gist at: https://gist.github.com/")
        return

    syncer = GameStateSync()
    syncer.run()

if __name__ == "__main__":
    main()
