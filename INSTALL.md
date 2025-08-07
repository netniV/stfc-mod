# Star Trek Fleet Command - Community Mod

<p align="center">
  <img src="https://img.shields.io/badge/License-GPLv3-blue.svg" alt="License: GPLv3">
  <img src="https://img.shields.io/github/sponsors/netniv" alt="Sponsorship">
</p>

<p align="center">
   A community mod (patch) for PC and Mac that adds a couple of tweaks to the mobile game <b>Star Trek Fleet Command&#8482;</b>
</p>

## Downloads / Releases
If you are looking to download the latest release of the STFC Community Mod, these are available on GitHub.

For the latest release only, see https://github.com/netniv/stfc-mod/releases/latest/
For both official and alpha/beta releases see https://github.com/netniv/stfc-mod/releases/

Note: GitHub may require you to login to see the downloads for pre-releases and you will need to expand the
      assets section to see the downloads.

Note: There is no difference between the versioned and unversioned zip files, they just allow people to store multiple
      versions rather than overwritting or having a random number appended by windows.

## Default file locations

`Game` refers to prime.exe and version.dll and the .log file. `Settings` refers to where the .vars and .toml files are located by
default in the following locations:

For PC:
- Game: `C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game`
- Settings: `C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game`

For Mac:
- Game: `~/Library/Application Support/Star Trek Fleet Command/Games/Star Trek Fleet Command/Star Trek Fleet Command/default/game/`
- Settings: `~/Library/Preferences/com.stfcmod.startrekpatch`

## Installation / Runtime

The DLL (once renamed version.dll) must be placed in the `Game` folder (see above) and called `version.dll`

If you have been provided with any default settings file, these would go to the `Settings` folder (see above) and would normally
 be called `community_patch_settings.toml`.

The pre-compiled DLL (PC) or DMG installer (Mac) can be downloaded from the official assets hosted on
[GitHub Releases](https://github.com/netniv/stfc-mod/releases)

For Macs, also check out the specific nuiances below.

For windows, an example of how to install on Windows can be found using [Lewb's video](https://youtu.be/3_5Jgk1CClU).

## Configuration

An example configuration file is [example_community_patch_settings.toml](example_community_patch_settings.toml) and should be
renamed to `community_patch_settings.toml`.  When running this file will be parsed (see `community_patch.log`) and the running
values can be found in `community_patch_runtime.vars`.  If you have any problems with a setting, check the log and parsed
file to verify that the setting was applied.

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
On windows, sometimes this is placed in a specific users profile folder.  This is suspected
to happen when it things more than one user attempted to install it, or that user doesn't have
admin rights.

To find the correct game location for the version.dll to be placed, the simpliest method is to
right click the prime.exe in the taskbar when it is running, then right click on prime.exe and
goto properties.  This will show the properties window and you can copy the game location from
there by pressing Ctrl+C or right click and copy.

With the path copied, you then paste it into File Explorer (windows+e) by clicking the address
bar at the top and pressing Ctrl+V or right-click and paste.

### Mac Specific

Apple likes to hide the preferences folder by default.  There are several ways to open the correct folder:

- Hold the Option key (or Alt on a PC keyboard) and click the Go to menu item and it’ll show you the Library folder

- Press Cmd+Shift+G in the Finder for the Goto box and enter `~/Library/Preferences/com.stfcmod.startrekpatch`

- Use terminal and type: `open ~/Library/Preferences/com.tashcan.startrekpatch`

Once you have the correct folder, you should find the file `community_patch_settings.toml` if you have run the mod at least once. See the [example_community_patch_settings.toml](example_community_patch_settings.toml) as a starting point.
