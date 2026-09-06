# Layout-aware letter shortcuts

`[control].keyboard_letter_mode = "physical"` preserves existing behavior (default).
Set it to `"layout"` to interpret configured A-Z by the active OS keyboard layout.
For example, on German QWERTZ `show_daily = "Z"` follows German Z, which occupies
the US-QWERTY Y position. Existing Y/Z workaround configurations should be undone
when opting in. Restart once to change this setting; subsequent OS layout changes
are detected in-game without restarting.

This is action binding, not text input. Shift/Ctrl/Alt chords retain their existing
semantics. Configured punctuation, digits, arrows, modifiers, hardcoded controls,
and native Scopely shortcuts are unchanged. Only configured A-Z are translated;
Unicode/dead-key/AltGr text composition is not supported. A letter can resolve to
a punctuation position (e.g. French M). Missing/unsupported letters are disabled
in layout mode, never silently treated as physical. Select physical and restart
to recover original behavior if the runtime API is unavailable.

## Implementation and provenance

`MapKey` preserves configured keys/text and resolves a letter just before querying
the existing physical `Key` cache. A single per-frame check reads Unity's current
keyboard and layout. The 26-letter table is rebuilt only when either changes.
Physical mode does not resolve Unity layout methods or poll keyboard layout state.
There are no new detours, native offsets, OS layout changes, input injection, or
key-event logging. A transition frame is suppressed; held resolved positions must
be released before activating a binding under the new layout.

Resolution uses Unity's `Keyboard.FindKeyOnCurrentKeyboardLayout` and translates
its `Key` enum explicitly to legacy `KeyCode`. These enums are **not** numerically
interchangeable. Unity documents physical key positions separately from
[layout-dependent display names](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.4/manual/Keyboard.html).
The Windows research session verified live US/German lookup changes and unchanged
legacy physical key reporting, including across a game restart. That evidence
does not constitute macOS or every-layout runtime validation.

The generated vars file preserves `[shortcuts]` and `[shortcuts_source]`.
`[keyboard_mapping]` reports mode, provider, layout, status, generation, reason,
scope and the US-reference position convention. `[shortcuts_resolved]` records
each parsed alternative with its configured chord (including modifiers), letter,
physical US-reference key, layout display name, legacy code and resolution status.
Startup is `pending` until a game-thread letter query can inspect the keyboard.
Derived vars are rewritten on mapping/status changes only, not every frame; no
user settings are rewritten. Transient hold suppression does not change the
resolved mapping and is not a vars generation. Missing Unity APIs or a managed
exception disable layout letters for that session; keyboard absence is retried.

## Validation

`xmake build keyboard-layout-tests` then `xmake run keyboard-layout-tests` exercises
the production conversion/transition core without launching STFC. In managed
workspaces use the local AX route for these commands.

Before release, live-test physical default and layout mode with US -> German -> US,
Y/Z actions, Ctrl/Shift chords, a held key during transition, typing/focus and vars
generation/provenance. French A/Q, W/Z and M test beyond a Y/Z-only implementation.
Build success and modelled unit tests do not prove platform runtime behavior.
