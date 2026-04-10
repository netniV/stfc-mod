# How-to: Set Up Game State Export and Gist Sync

**Type:** How-to guide  
**Audience:** Players installing the mod and configuring game state export for the first time.

---

## Prerequisites

- STFC Community Mod installed as `version.dll` in your game directory
- A GitHub account
- A GitHub Personal Access Token with `gist` scope ([create one here](https://github.com/settings/tokens))

---

## Step 1: Enable game state export in the config

Open `community_patch_settings.toml` in your game directory and add:

```toml
[sync.game_state]
enabled = true
interval = 300        # export every 5 minutes; lower values export more frequently
on_startup = true     # export immediately when you log in
player_id = ''        # set to your STFC user ID for accurate player name resolution
path = ''             # leave empty to default to the game directory
```

---

## Step 2: Create a GitHub Gist

1. Go to [gist.github.com](https://gist.github.com) and create a new Gist.
2. Add a single placeholder file (the mod will populate it on first run).
3. Copy the Gist ID from the URL: `gist.github.com/USERNAME/GISTID`.

---

## Step 3: Configure Gist sync

Add the following section to `community_patch_settings.toml`:

```toml
[sync.game_state.github]
enabled  = true
gist_id  = 'YOUR_GIST_ID'
username = 'YOUR_GITHUB_USERNAME'
token    = 'YOUR_GITHUB_TOKEN'
```

> **Tip:** The `username` field is used to construct raw URLs in the manifest. If left empty the mod resolves it automatically from the GitHub API, but setting it explicitly avoids an extra API call on startup.

---

## Step 4: Launch the game and verify

1. Start the game. The mod exports on startup if `on_startup = true`.
2. Open your Gist. Within a minute you should see these files appear:

| Gist file | Contents |
|-----------|----------|
| `stfc_manifest.json` | Index of all export files with raw URLs |
| `stfc_player.json` | Player profile, station, drydocks, peace shield, queues |
| `stfc_ships.json` | Ships in hangar with tier, level, cargo stats |
| `stfc_resources.json` | All resources and inventory |
| `stfc_research.json` | Unlocked research nodes |
| `stfc_officers.json` | Officer roster with rank, level, shards, traits |
| `stfc_buildings.json` | Station buildings and levels |
| `stfc_faction.json` | Faction reputation, loyalty buffs, faction favors |
| `stfc_buffs.json` | Active buff catalog |
| `stfc_territory.json` | Territory takeover windows |
| `stfc_missions.json` | Mission progress |
| `stfc_summary.json` | Compact pre-computed digest for token-efficient AI use |

3. Check `community_patch.log` in the game directory if files do not appear — it will show sync errors.

---

## Step 5: Use the viewer

Open the viewer at `https://drcord.github.io/stfc-mod/` and paste the raw base URL from `stfc_manifest.json`. The viewer shows your ships, officers, research, resources, buildings, and territory in sortable/filterable tables.

---

## Step 6: Use with an AI assistant

Point your AI assistant at the summary file for token-efficient access:

```
I'm playing Star Trek Fleet Command. My current game state summary is at:
https://gist.githubusercontent.com/USERNAME/GISTID/raw/stfc_summary.json

Please fetch this before giving advice. For detailed data (full ship list,
officer roster, etc.) the file index is at:
https://gist.githubusercontent.com/USERNAME/GISTID/raw/stfc_manifest.json
```

Use `stfc_summary.json` for general planning questions. Use individual files
(e.g. `stfc_officers.json`) when the AI needs complete detail on a specific category.

---

## Troubleshooting

| Symptom | Check |
|---------|-------|
| No files in Gist | `community_patch.log` — look for `Gist sync` errors |
| Files appear but are empty | Ensure `[sync.game_state] enabled = true` and the game has fully loaded |
| Token errors | Confirm token has `gist` scope and has not expired |
| Wrong player name | Set `player_id` in config to your STFC user ID |
