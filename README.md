# Star Trek Fleet Command - Community Patch

<p align="center">
  <img src="https://img.shields.io/badge/License-GPLv3-blue.svg" alt="License: GPLv3">
  <img src="https://img.shields.io/github/sponsors/netniv" alt="Sponsorship">
</p>

<p align="center">
   A community patch that adds a couple of tweaks to the mobile game <b>Star Trek Fleet Command&#8482;</b>
</p>

## Features

- Set system UI scale + adjustment factor
- Set viewer UI scale
- Set system zoom
  - default
  - maximum
  - keyboard speed
  - presets (1-5)
- Set transition time
- Disable various toast banners
- Disable galaxy chat
- Enable/Disable hotkeys (community mod or scopely)
- Enable extended donation slider (alliance)
- Show alternative cargo screens for:
  - default
  - player
  - station
  - hostile
  - armada
- Press ESCAPE to remove pre-scan viewers
- Skip reveal sections when opening chests
- Exit section when collecting gifts
- Create default toml file settings file if none exists
- Create parsed toml file to show what settings have been applied
- Customise your keyboard shortcuts

## Keyboard shortcuts

The following are default shortcuts that can be modified (see [KEYMAPPING.md](KEYMAPPING.md))

Key | Shortcut
--: | ---
SPACE | Perform default action
1-8 | Ship select/focus
F1-F5 | Zoom presets
Q | Zoom Out
E | Zoom In
T | Events
G | Galaxy
H | System
Shift-G | Exterior View
Shfit-H | Interior View
R | When ship selected, recall ship
R | When clicking on mine/player/enemy, perform non-default action (eg, scan)
V | When clicking on mine/player/enemy, toggle view of cargo or default screen
C | Focus Chat (or Open Alliance Chat - Full Screen)
Alt-C | Focus Chat (or Open Alliance Chat - Side of Screen)
PGUP | UI Scale Up
PGDOWN | UI Scale Down

The following keybinds have recently been added:

> [!NOTE]
> Can be disabled via `hotkeys_extended` config option

Key | Shortcut
--: | ---
MINUS | Zoom (min)
EQUALS | Zoom (default)
BACKSPACE | Zoom (max)
B | Bookmarks
F | Factions
Shift-F | Refinery
Shift-I | Artifact Gallery
U | Research
I | Inventory
M | Active Missions
O | Command Center
Shift-O | Officers
Shift-Q | Q-Trials
Shift-T | Away Teams
X | ExoComp
Z | Daily Missions
` | Open Alliance Chat - Side of Screen
SHIFT-PGUP | UI Viewer Scale Up
SHIFT-PGDOWN | UI Viewer Scale Down

## Contributing / Building

If you wish to contribute to the project, or simply compile the DLL yourself, please see [CONTRIBUTING.md](CONTRIBUTING.md)

## Installation / Runtime

Both the DLL and the toml file must be placed in the STFC game folder which is always located
at `C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game` as the following
names:

- community_patch_settings.toml
- version.dll

The pre-compiled DLL can be downloaded from the official assets hosted on [GitHub Releases](https://github.com/netniv/stfc-mod/releases)

## Configuration

An example configuration file is [example_community_patch_settings.toml](example_community_patch_settings.toml) and should be
renamed to `community_patch_settings.toml`.  When running this file will be parsed (see `community_patch.log`) and the running
values can be found in `community_patch_runtime.vars`.  If you have any problems with a setting, check the log and parsed
file to verify that the setting was applied.

## Problems?

The most common problems getting the DLL to work are:

1. Not installed in the correct location.  This must be:

   ```console
   C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game
   ```

2. Windows is blocking the DLL.  Right-click the file and select Properties.  On the `General` tab
   there will be additional text at the bottom:

   ```console
   This file can from another
   computer and might be blocked to
   help protect this computer
   ```

   To the right of this, there will be a tick box called `Unblock`.  Tick the box and then click OK
   to unblock the file.

3. The configuration file has the wrong name (see above)

4. The configuration file is not being parsed as you expect which is normally because:

   - Your configuration isn't being parsed
   - The configuration option name is spelt wrong
   - The configuration option name is in the wrong section
   - The configuration option value is not a true or false

   You can verify your configuration by looking at `community_patch_runtime.vars` and/or the
   log file `community_patch.log`.

## Support

For STFC Community Mod items, please visit the [STFC Community Mod](https://discord.gg/PrpHgs7Vjs) discord server.

Tashcan has now retired all things STFC from [Ripper's Corner](https://discord.gg/gPuQ5sPYM9) but still swing by to say hello to the wonderful man.

## Disclaimer

This is intended to give people insight and possiblity to add new things for QoL improvements.

There is no guarantee or promise that using this for features outside of what is officially offered via this repository will not result actions against your account.

All features and additions provided here via this repository are sanctioned by Scopely and thus aren't subject to account actions.

## License

- GPLv3
