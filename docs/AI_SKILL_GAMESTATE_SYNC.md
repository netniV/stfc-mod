# AI Skill: STFC Game State Sync

## What This Does

Automatically exports your Star Trek Fleet Command game state to a JSON file while you play. This JSON file can be synced to GitHub (via Gist or repository) so any AI assistant (Claude, ChatGPT, etc.) can instantly access your current game state for planning and advice.

## Setup

### Step 1: Install the Community Mod

1. Download the latest `stfc-community-mod` DLL from releases
2. Copy `stfc-community-mod.dll` to your STFC game directory as `version.dll`:
   ```
   C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game\version.dll
   ```

### Step 2: Enable Game State Export

Edit the `community_patch_settings.toml` file in the same directory and add:

```toml
[gamestate_export]
enabled = true
interval = 300                  # Export every 5 minutes (300 seconds)
path = ""                       # Leave empty to use game directory
export_on_startup = true        # Export when game starts
```

### Step 3: Set Up GitHub Sync (Optional but Recommended)

**Option A: GitHub Gist** (Easiest)

1. Create a new GitHub Gist (secret or public) at https://gist.github.com/
2. Get a Personal Access Token with `gist` scope from https://github.com/settings/tokens
3. Use the Python sync script (see below)

### Step 4: Use with AI

When talking to Claude or ChatGPT, start your conversation with:

```
I'm playing Star Trek Fleet Command. My current game state is available at:
https://gist.githubusercontent.com/yourusername/{gist_id}/raw/stfc_gamestate.json

Please fetch this to understand my current game state before giving advice.
```

## Data Format

The exported JSON follows this structure:

```json
{
  "exported_at": "2026-04-03T09:00:00Z",
  "export_version": "1.0",
  "mod_version": "stfc-community-mod-1.1.0",
  "player": {
    "ops_level": 41,
    "server": 709,
    "player_id": "12345",
    "player_name": "DrCord"
  },
  "buildings": [...],
  "research": [...],
  "ships": [...],
  "faction_rep": [...],
  "resources": {...},
  "bp_progress": [...]
}
```

## Benefits

? **No Manual Screenshots** - AI always has current data  
? **Real-Time Sync** - Updates every 5 minutes (configurable)  
? **Privacy Control** - Use private Gists or repos  
? **Cross-AI Compatible** - Works with Claude, ChatGPT, any AI that can fetch URLs  
? **Historical Tracking** - Git history shows your progression  
? **Easy Sharing** - Share with alliance members or friends  

## Troubleshooting

**Export file not created?**
- Check `community_patch.log` in game directory for errors
- Verify `enabled = true` in `[gamestate_export]` section
- Check file permissions

**Export is empty or has zeros?**
- The initial implementation provides the structure - game data hooks are being developed
- Check for updates to the mod for enhanced data export
- Contribute to the project to add more data fields!

**Sync script not working?**
- Verify GitHub token has `gist` scope
- Check Python dependencies: `pip install requests`
- Verify file path is correct

## Contributing

This feature is part of the STFC Community Mod. If you find bugs or want to add more data fields:

1. Fork the repo: https://github.com/DrCord/stfc-mod
2. Work on `dev` branch
3. Submit PR to upstream: https://github.com/netniV/stfc-mod

Join the mod Discord: https://discord.gg/PrpHgs7Vjs
