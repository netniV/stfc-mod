# Star Trek Fleet Command - Community Mod

<p align="center">
  <img src="https://img.shields.io/badge/License-GPLv3-blue.svg" alt="License: GPLv3">
  <img src="https://img.shields.io/github/sponsors/netniv" alt="Sponsorship">
</p>

<p align="center">
   A community mod (patch) for PC and Mac that adds a couple of tweaks to the <b>Star Trek Fleet Command&#8482;</b> game
</p>

## Downloads / Releases

The STFC Community Mod is available on GitHub.

For the latest official release only, see [https://github.com/netniv/stfc-mod/releases/latest/]

For both official and alpha/beta releases, see [https://github.com/netniv/stfc-mod/releases/]

Note: GitHub may require you to login to see the downloads for pre-releases and you will need to expand the
      assets section to see the downloads.

Note: There is no difference between the versioned and unversioned zip files, they just allow people to store multiple
      versions rather than overwritting or having a random number appended by windows.

## Installation

### Windows

1) Download `stfc-community-patch.zip` file from a github release and extract the `version.dll` file.

2) The default folder for both the game and the settings file is
`C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game`
(if this folder isn't present, see the [windows-specific](#windows-specific) section below.)  Open this folder in Explorer.

3) Place the `version.dll` file from the downloaded .zip file in this folder.

4) If installing the mod for the first time, save the
[sample configuration file](example_community_patch_settings.toml)
to this folder __and rename it__ to `community_patch_settings.toml`.

### Mac

1) Download the `stfc-community-patch-installer.dmg` file from a github release, then open it and drag and drop the launcher to your Applications folder.  This launcher will be used to start the game any time you want to have the mod loaded.

2) The patch settings are stored in `~/Library/Preferences/com.stfcmod.startrekpatch`. If installing the mod for the first time, save the [sample configuration file](example_community_patch_settings.toml)
to this folder __and rename it__ to `community_patch_settings.toml` (if your Library folder is not visible, see the [mac-specific](#mac-specific) section below).

The STFC game itself is located in the `~/Library/Application Support/Star Trek Fleet Command/Games/Star Trek Fleet Command/STFC/default/game` folder.  You should only need to access this folder if you need to view the `community_patch.log` file.

### Wine/Linux

The Windows version of the mod, and the game itself, run well under Linux using the Wine compatability layer.  If the game is installed using the [STFC installer for the Lutris game manager](https://lutris.net/games/star-trek-fleet-command/), the mod can be installed by following the [Windows](#windows) directions above and using the `~/Games/star-trek-fleet-command/drive_c/Games/Star Trek Fleet Command/Star Trek Fleet Command/default/game` directory for installation.

If the game is installed through other launchers or directly in a WINEPREFIX, an additional adjustment may be needed for it to load the mod  library correctly; see the [wine/linux specific](#winelinux-specific) notes below.

## Configuration

Make any and all configuration changes in the `community_patch_settings.toml` file _only_. When the game is
launched, this file will be read, and the parsed values for all settings, including default values for
settings not in the _.toml_ file, will appear in `community_patch_runtime.vars`.

If you have any problems with a setting,
check the parsed value in the _.vars_ file to verify that the setting was applied, and check the
`community_patch.log` file to see if any errors were encountered while parsing the _.toml_ file.

## Problems?

The most common problems getting the DLL to work are:

1. Not installed in the correct location.  This must be `Game` folder where `prime.exe` also exists.

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

### Windows specific

On windows, sometimes the game is installed in a specific users profile folder.  This is suspected
to happen when it thinks more than one user attempted to install it, or if that user doesn't have
admin rights.

If this is the case, to find the correct game location for the `version.dll` file, do the following:

1) Open Task Manager while the game is running (press Ctrl-Alt-Delete)
2) Right-click `prime.exe` and select 'Expand'
3) Right-click `Star Trek Fleet Command` and select 'Open file location'

An Explorer window will open with the correct folder selected.

### Mac Specific

Apple likes to hide the `Library` folder by default. There are several ways to open the correct folder:

- Hold the `⌥ option` key (or `Alt` on a PC keyboard) in __Finder__ and click the __Go to__ menu item and then click the __Library__ folder item
- Press `⌘ cmd`+`⇧ shift`+`G` in the __Finder__ for the Goto box and type in `~/Library`
- Use __Terminal__ and type: `open ~/Library`

You need to have run the game with the mod at least once for any settings (toml), runtime variables (.vars) or log files (.log) to appear.  Then, once you have the correct folder, you should find the file `community_patch_settings.toml` . See the [`example_community_patch_settings.toml`](example_community_patch_settings.toml) as a starting point.

### Wine/Linux Specific

To use the Windows version of the mod under Wine (generally Linux), the wine DLL overrides setting _must_ be set to `n,b` for `version.dll` or it will not be loaded.  If you have installed the game using the [STFC installer for the Lutris game manager](https://lutris.net/games/star-trek-fleet-command/) (recommended), this has already been set up in the runner configuration.  Otherwise, it can be set in the `winecfg.exe` Libraries tab or by setting the `WINEDLLOVERRIDES` enviroment variable to `version.dll=n,b` before launching the game.

Wine mod installation generally follows the Windows directions above; just place the `version.dll` in the `drive_c/Games/Star Trek Fleet Command/Star Trek Fleet Command/default/game` directory relative to the wine environment (WINEPREFIX) you're running it under.
