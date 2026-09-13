# Layout-aware shortcuts (Windows x64)

The default `[control].keyboard_layout_mode = "physical"` preserves existing behavior.
Set it to `"layout"` and restart to interpret printable shortcut characters using
Windows' active keyboard layout. Subsequent layout changes apply live through
Unity device notifications. Release held keys before using the new layout.

For a German layout:

| Configured chord | Keys on a German keyboard |
| --- | --- |
| `/` | Shift+7 |
| `CTRL-'` | Ctrl+Shift+# |
| `ALT-^` | Alt+^ |
| `SHIFT-^` | Shift+^ |
| `Z` | Z (US physical Y position) |

Remove old Y/Z workarounds when opting in. Uppercase letter tokens name keys;
they do not request Shift. Shift needed to produce punctuation is added to the
explicit configured modifiers. Side-specific modifiers retain their requirements.
Existing explicitly modified bindings accept extra modifiers; avoid overlapping
chords, since action dispatch order determines which matching action wins.

**German users enabling experimental shortcuts:** the upstream defaults
`show_alliance_help = "SHIFT-'"` and `show_alliance_armada = "CTRL-'"` overlap
because German apostrophe already requires Shift. Help is checked first and
captures the Armada chord. Use the tested remap below in the existing sections:

```toml
[control]
keyboard_layout_mode = "layout"
enable_experimental = true

[shortcuts]
show_alliance = "ALT-^"
show_alliance_help = "SHIFT-^"
show_alliance_armada = "CTRL-'"
```

This keeps upstream modifier matching and shortcut defaults unchanged.

Hints and runtime shortcut values retain configured TOML text. The internal
physical mapping is not a replacement hint. Named controls (function keys, arrows,
Space, mouse and numpad keys), hardcoded controls and Scopely shortcuts are unchanged.
Alliance Help and Armada retain the upstream `enable_experimental = true` requirement.

The resolver uses Windows character-to-key translation and supported scan-code
positions. Unshifted dead keys use a non-composing fallback: no following Space
is needed for a shortcut, and lookup does not consume pending text accents.
Characters requiring inferred Ctrl/Alt/AltGr, unsupported positions and missing
characters are disabled individually. Explicit configured Ctrl/Alt still work.

Mapping is cached per requested character and refreshed only at initialization or
a device notification. Held keys are suppressed through a layout transition until
released. Notification failure disables layout bindings until restart; there is no
polling or silent physical fallback. The runtime `[keyboard_mapping]` section gives
layout, generation and status; logs identify unresolved characters.

Layout mode is currently supported on Windows x64. Other platforms retain default
physical behavior; explicitly requesting layout mode disables printable bindings
and reports `platform_unsupported`. The experimental macOS adapter and prototype
preview/diagnostic settings are not part of this implementation.
