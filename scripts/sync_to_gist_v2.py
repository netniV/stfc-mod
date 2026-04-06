#!/usr/bin/env python3
"""
STFC Game State to GitHub Gist Sync Script v2.0

Automatically syncs your STFC community mod gamestate JSON files (full + differential)
to a GitHub Gist for easy AI access.

Setup:
1. Create a GitHub Personal Access Token with 'gist' scope:
   https://github.com/settings/tokens

2. Create a new Gist (can be secret):
   https://gist.github.com/

3. Edit the CONFIG section below with your details

4. Run: python sync_to_gist_v2.py
"""

import requests
import time
import json
import hashlib
from pathlib import Path
from datetime import datetime

# Import credentials from separate config file (not committed to git)
try:
    from gist_config import GITHUB_TOKEN, GIST_ID
except ImportError:
    # Fallback to placeholders if config file doesn't exist
    GITHUB_TOKEN = "ghp_YOUR_TOKEN_HERE"
    GIST_ID = "YOUR_GIST_ID_HERE"
    print("WARNING: gist_config.py not found. Please create it with your credentials.")
    print("See gist_config.py.example for template.")

# ============================================================================
# CONFIG - EDIT gist_config.py (not committed to git)
# ============================================================================

# GitHub credentials are now imported from gist_config.py
# Create that file with:
#   GITHUB_TOKEN = "your_token_here"
#   GIST_ID = "your_gist_id_here"

# Paths to your STFC game state JSON files
GAMESTATE_FULL_FILE = r"C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game\community_patch_gamestate.json"
GAMESTATE_DELTA_FILE = r"C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game\community_patch_gamestate_delta.json"

# How often to check for changes (seconds)
CHECK_INTERVAL = 30

# Filenames in the Gist
GIST_FILENAME_FULL = "stfc_gamestate_full.json"
GIST_FILENAME_DELTA = "stfc_gamestate_delta.json"

# ============================================================================
# NO NEED TO EDIT BELOW THIS LINE
# ============================================================================

class GameStateSync:
    def __init__(self):
        self.last_full_hash = None
        self.last_delta_hash = None
        self.gamestate_full_path = Path(GAMESTATE_FULL_FILE)
        self.gamestate_delta_path = Path(GAMESTATE_DELTA_FILE)
        self.api_url = f"https://api.github.com/gists/{GIST_ID}"
        self.headers = {
            "Authorization": f"token {GITHUB_TOKEN}",
            "Accept": "application/vnd.github.v3+json"
        }

    def calculate_file_hash(self, file_path):
        """Calculate MD5 hash of file to detect changes"""
        if not file_path.exists():
            return None

        with open(file_path, 'rb') as f:
            return hashlib.md5(f.read()).hexdigest()

    def validate_json(self, file_path):
        """Validate that the file contains valid JSON"""
        try:
            with open(file_path, 'r') as f:
                json.load(f)
            return True
        except json.JSONDecodeError as e:
            print(f"??  Invalid JSON in {file_path.name}: {e}", flush=True)
            return False
        except FileNotFoundError:
            return False

    def update_gist(self, file_path, gist_filename):
        """Upload a gamestate file to Gist"""
        try:
            with open(file_path, 'r') as f:
                content = f.read()

            # Parse to get metadata for logging
            try:
                data = json.loads(content)

                # Handle delta array format
                if isinstance(data, list):
                    export_type = 'differential_array'
                    total_deltas = len(data)
                    if total_deltas > 0:
                        latest = data[-1]
                        exported_at = latest.get('exported_at', 'unknown')
                        total_changes = latest.get('summary', {}).get('total_changes', 0)
                    else:
                        exported_at = 'empty'
                        total_changes = 0
                else:
                    exported_at = data.get('exported_at', 'unknown')
                    export_type = data.get('export_type', 'unknown')
                    total_changes = data.get('summary', {}).get('total_changes', 0) if export_type == 'differential' else None
                    total_deltas = None

            except:
                exported_at = 'unknown'
                export_type = 'unknown'
                total_changes = None
                total_deltas = None

            payload = {
                "files": {
                    gist_filename: {
                        "content": content
                    }
                }
            }

            response = requests.patch(self.api_url, headers=self.headers, json=payload)

            if response.status_code == 200:
                now = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
                if export_type == 'differential_array':
                    print(f"? [{now}] Synced DELTA ARRAY ({total_deltas} deltas, latest: {total_changes} changes)", flush=True)
                elif export_type == 'differential' and total_changes is not None:
                    print(f"? [{now}] Synced DIFFERENTIAL export ({total_changes} changes)", flush=True)
                else:
                    print(f"? [{now}] Synced FULL export", flush=True)
                return True
            else:
                print(f"? Failed to update Gist: HTTP {response.status_code}", flush=True)
                print(f"   Response: {response.text}", flush=True)
                return False

        except FileNotFoundError:
            # File not found is expected - differential might not exist yet
            return False
        except requests.RequestException as e:
            print(f"? Network error: {e}", flush=True)
            return False
        except Exception as e:
            print(f"? Unexpected error: {e}", flush=True)
            return False

    def get_gist_urls(self):
        """Get the raw URLs for the Gist files (latest version, no commit hash)"""
        # Return URLs without commit hashes so they always point to latest version
        return {
            'full': f"https://gist.githubusercontent.com/DrCord/{GIST_ID}/raw/{GIST_FILENAME_FULL}",
            'delta': f"https://gist.githubusercontent.com/DrCord/{GIST_ID}/raw/{GIST_FILENAME_DELTA}"
        }

    def run(self):
        """Main sync loop"""
        print("?? STFC Game State ? GitHub Gist Sync v2.0", flush=True)
        print(f"?? Watching:", flush=True)
        print(f"   Full:  {self.gamestate_full_path}", flush=True)
        print(f"   Delta: {self.gamestate_delta_path}", flush=True)
        print(f"?? Gist ID: {GIST_ID}", flush=True)
        print(f"??  Check interval: {CHECK_INTERVAL}s", flush=True)
        print("\n" + "="*60, flush=True)

        # Check if files exist
        if not self.gamestate_full_path.exists():
            print(f"\n??  WARNING: Full gamestate file not found!", flush=True)
            print(f"   Make sure STFC is running with the community mod installed", flush=True)
            print(f"   and enabled = true in [gamestate_export] section\n", flush=True)

        urls = self.get_gist_urls()
        print("\n?? Share these URLs with AI assistants:", flush=True)
        print(f"   Full:  {urls.get('full', 'N/A')}", flush=True)
        print(f"   Delta: {urls.get('delta', 'N/A')}\n", flush=True)
        print("Press Ctrl+C to stop\n", flush=True)

        try:
            while True:
                # Check and sync full export
                if self.gamestate_full_path.exists():
                    current_full_hash = self.calculate_file_hash(self.gamestate_full_path)

                    if current_full_hash != self.last_full_hash:
                        if self.validate_json(self.gamestate_full_path):
                            if self.update_gist(self.gamestate_full_path, GIST_FILENAME_FULL):
                                self.last_full_hash = current_full_hash

                # Check and sync differential export
                if self.gamestate_delta_path.exists():
                    current_delta_hash = self.calculate_file_hash(self.gamestate_delta_path)

                    if current_delta_hash != self.last_delta_hash:
                        if self.validate_json(self.gamestate_delta_path):
                            if self.update_gist(self.gamestate_delta_path, GIST_FILENAME_DELTA):
                                self.last_delta_hash = current_delta_hash

                time.sleep(CHECK_INTERVAL)

        except KeyboardInterrupt:
            print("\n\n?? Sync stopped by user", flush=True)
        except Exception as e:
            print(f"\n? Fatal error: {e}", flush=True)

def main():
    # Validate configuration
    if GITHUB_TOKEN == "ghp_YOUR_TOKEN_HERE":
        print("? ERROR: Please edit the script and set your GITHUB_TOKEN", flush=True)
        print("   Get a token from: https://github.com/settings/tokens", flush=True)
        return

    if GIST_ID == "YOUR_GIST_ID_HERE":
        print("? ERROR: Please edit the script and set your GIST_ID", flush=True)
        print("   Create a gist at: https://gist.github.com/", flush=True)
        return

    syncer = GameStateSync()
    syncer.run()

if __name__ == "__main__":
    main()
