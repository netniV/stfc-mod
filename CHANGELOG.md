# Star Trek Fleet Command - Community Mod

<p align="center">
  <img src="https://img.shields.io/badge/License-GPLv3-blue.svg" alt="License: GPLv3">
  <img src="https://img.shields.io/github/sponsors/netniv" alt="Sponsorship">
</p>

<p align="center">
   A community mod (patch) that adds a couple of tweaks to the mobile game <b>Star Trek Fleet Command&#8482;</b>
</p>

# Change Log

## 0.6.1

### Features
- Added support for macOS
- Added maximum donation for alliance sliders
- Added `ui_scale_viewer` to scale the size of object viewers
- Added `select_current` hotkey to locate currently selected ship in dock
- Added `UiViewerScaleUp` and `UiViewerScaleDown` hotkeys to scale `ui_scale_viewer` in game
- Added `toggle_queue`, `action_queue` and `action_queue_cancel` for Kir'Shira artifact handling
- Added `action_quit` for instant game exit
- Added prefix to title when using `-ccm` command line option to show which is being used [windows only]
- Added `select_timer` to adjust period in which double tap ship selection works [default 500ms]
- Added suppression of first message notification
- Updated sync methods to:
  - allow multi-targets
  - better handle errors
  - include missions
  - include officer shard counts
  - better handle battlelogs

### Fixes:
- Fixed `ui_scale` or `ui_scale_viewer` so when set to 0.0f disable those scaling features
- Fixed `zoom_min` and `zoom_max` no longer set default system zoom (only presets can)
- Fixed numerous method hooks to report when they are not found rather than crash
- Fixed issue where a widget may not have a visibility controller for some reason causing a crash
- Fixed warp cancellation so it works again

### Dev stuff:
- Switch to xmake
- log names of log, configuration, vars and battlelog files
- update github source to netniV's repo

## 0.6.0

- Add customisable hotkeys
- Add example configuration file
- Make ui_scaleup/ui_scaldown operate like zoom and can now be held
- Add ui_scale_adjust to allow changes to "step" between ui_scale's
- Add option to disable:
  - Ship locate when preview window open
  - Ending program via escape
  - Movement via keyboard
- Add session-based adjustments that last until game is restarted:
  - Add toggle cargo views (ALT 1-5)
  - Add set zoom preset (SHIFT F1-F5)
- Fix speed issues with ObjectFinder by writing our own
- Adjust runtime configuration output:
  - Rename 'community_patch_settings_parsed.toml' to 'community_patch_runtime.vars' to avoid confusion
  - Add a massive comment to top of community_patch_runtimes.vars
  - When possible report bad configuration values
- Add new key mappings:
  - N: Manage Ship
  - R: Repair Ship
  - U: Show Resarch
  - CTRL-ALT-MINUS: Disable all hotkeys
  - CTRL-ALT-EQUAL: Enable all hotkeys
  - F9: Log level to Debug
  - F11: Log level to Info
  - SHIFT-T: Show Away Team
  - SLASH (Forward): Show Gifts
  - SLASH (Backward): Show Alliance
  - CTRL-L: Toggle whether Locate will work when preview popup visible
  - CTRL-R: Toggle whether Recall will work when preview popup visible
  - ALT-1: Toggle whether Cargo will show for Default
  - ALT-2: Toggle whether Cargo will show for Player
  - ALT-3: Toggle whether Cargo will show for Station
  - ALT-4: Toggle whether Cargo will show for Hostile
  - ALT-5: Toggle whether Cargo will show for Armada
- Add new key mappings (alpha testing only due to implementation issues):
  - SHIFT-SLASH (Backward): Show Alliance Help
  - CTRL-SLASH (Backward): Show Alliance Armada
  - l: Show Lookup

## 0.5.2

- fix free resize not working anymore
- restrict the stay in bundle after summary behavior to officer and event store
- add player names to battle log sync
- fix trait sync not working
- add sync of active missions

## 0.5.0 (future)

- add default toml config when none exists with default values

- add parsed toml config output to show what settings are in use

- add zoom shortcuts (minus, equals, backspace) to provide max, default and minimum zoom

- add more keyboard shortcuts for missions (daily, away, etc), station views, quick chat

## 0.4.0

- who knows

## 0.3.8

- add drydock H hotkey

## 0.3.2

- add compatibility with Unity 2020.3 LTS

## 0.3.1

- add option to disable galaxy chat
- fix a bug where [Space] didn't trigger correctly

## 0.3.0

- add recall option, this is bound to `R` when not having another item selected that has an `R` action
- remove fix for iss jellyfish as this is now fixed in the regular client (`fix_iss_jellyfish_warp` config no longer has any effect)
- remove fix for stella as this is now fixed in the regular client (`fix_stella` config no longer has any effect)
- remove fix for faction as this is now fixed in regular client (`fix_faction` config no longer has any effect)
- re-enable free resizing of window regardless of aspect ratio
- extend alliance donation slider to be the amount required to reach the next alliance level
- extend ship selection hotkeys to 7

# 0.2.0

- initial release
