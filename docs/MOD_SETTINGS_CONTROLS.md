# Mod settings controls

Build real controls on the navigation foundation in small slices. Register only
working controls; omit empty groups. Stable setting keys and storage owners stay
independent of labels and placement.

See [the current architecture contract](MOD_SETTINGS.md) and
[native adapter ownership](MOD_SETTINGS_NATIVE_ADAPTER.md).

## Layout

| Location | Control | Existing owner |
| --- | --- | --- |
| Mod Settings > Map & Travel | Instant warp mode: Normal (ask), Warp, Jump | `ui.auto_confirm_instant_warp` and the Alt+I action |
| Mod Settings > Fleet Labels | Player label detail and zoom threshold | `graphics.zoom_label_player_detail`, `graphics.zoom_label_player_threshold` |
| Mod Settings > Fleet Labels | Non-player label detail and zoom threshold | `graphics.zoom_label_non_player_detail`, `graphics.zoom_label_non_player_threshold` |
| Mod Settings > Camera | Keyboard zoom speed and pan glide | `graphics.keyboard_zoom_speed`, `graphics.system_pan_momentum_falloff` |
| Mod Settings > Previews & Cargo | Locate/Recall while previewing; automatic cargo and target types | Existing preview/cargo keys in `[ui]` |
| Future separate branch: Hotkeys | Rebind existing actions | Existing shortcut parser and `MapKey` registrations |
| General > confirmation page | Confirm Forbidden Tech upgrades | Inverse of `ui.auto_confirm_ft_upgrade` |

The controls branch implements these controls on Windows x64.
Hotkey editing remains a separate branch. Native confirmation
controls stay on the native page. FC retains its existing owner.

## Instant warp mode

Use one selection, not three independently stored flags. The UI and Alt+I call
the same live mutation function. Cycle order remains Normal > Warp > Jump > Normal.
Selecting the current value does not enqueue another save. Invalid choices do
not alter live state or the file. Existing per-ship overrides retain precedence;
the picker changes only the global fallback mode.

Reuse the existing single runtime writer, optimistic conflict handling and
source-preserving TOML edits. UI readback confirms the live value, not durable
storage; asynchronous failures produce a quiet session-only notice, with details
in the log. Reopening must
read the current owner, and shortcut changes must refresh a visible selector.
Native selection callbacks need the same rendering, stale-context and reentry
protection already exercised for boolean controls.

Selected options use bold text and the native checkmark on a normal background,
including instant warp and both Fleet Labels profiles. White fill is transient
pressed feedback, not persistent selection or keyboard focus. The scoped adapter
uses native sprites already rendered by settings rows and restores each Image's
previous override before pooling. A Windows-only `Selectable.DoStateTransition`
hook observes input-state changes, calls the original once, then updates only
owned selection rows. Other controls take the native path; there is no frame
polling, animation replacement, asset loading or setting write in this hook.

## Fleet Labels and Forbidden Tech

One Fleet Labels page contains a collapsible Player heading, its Native /
Expanded / Compact / Threshold choices and percentage slider, followed by the
same controls under a Non-player heading. Each profile has its own owner and
selection. Click either heading to hide/show its controls independently, without
navigating away. Both sections start collapsed on each page visit; expansion is
temporary presentation state and never writes TOML or changes a setting value.
The native category arrow points down when expanded and right when collapsed.
Headings use larger bold cyan text and a darkened row background. An enabled
threshold slider uses a subtle cyan accent to connect it to the selected mode,
without a white selection fill. Tints affect the row's direct `BG` Image child
(`Background` for category headings),
when present; other prefab layouts retain the text styling. Native colors and
text are restored before refresh and pooling. Styling uses the existing bind,
refresh and release hooks, with no frame polling or shared-material changes.
Threshold is stored in [0, 1], edited in 1% steps,
and enabled only in Threshold mode. At 0% labels stay compact; at 100% they stay
expanded. Reading a player-authored fractional value does not round or save it.
Each user edit updates the existing live profile and refreshes tracked labels.
The native slider callbacks use the same typed snapshot/reentry guards as choices.
Unknown values suppress the slider and numeric label; disabled known values remain
visible. Releasing a pooled widget restores its label, active state and interaction.

Windows installs the existing fleet-label and Forbidden Tech hooks when the mod
settings UI is enabled, so changing their values does not require a restart.
Each FT hook consults the current bypass flag; hook availability is separate from
the value. Other platforms retain startup-controlled installation and omit this UI.
Confirmation ON means the bypass flag is false. Toggling must never invoke an
upgrade callback by itself.

The existing TOML writer now registers these additional keys at startup. One
worker serializes changes to the same file, keeping the latest pending intent
**per key**. A 150 ms quiet period coalesces slider motion; normal quit flushes the
pending value without waiting out that delay. F10 retains its existing force-close
cancellation and 500 ms best effort bound. Save failures/conflicts log the affected
section and key and leave the live setting in place. Numeric edits use the TOML
serializer and the same source-preserving edit/reparse/external-edit checks.
Page opens, section folding and native rendering never enqueue saves.

Collapsible headings reuse the existing category bind/release and page-selection
hooks. A heading click gives the native option panel a filtered `OptionContext[]`
through the existing `BindDataContext(provider, object)` virtual slot. The original page's
children and navigation parent stay intact, including controls omitted from the
visible list. Native rebinding releases hidden widgets and refreshes expanded
ones through the same guarded readers as a normal page visit. Plain headings
remain non-interactive. No new detour, frame polling or persistence owner is added.

On entry, native navigation establishes the selected page and Back target first;
the same callback then applies the initial collapsed list before returning. If
that presentation bind fails, the adapter attempts to restore the expanded list
so controls remain accessible. Expanding either section reads its current values.

Exact Windows build261 unwind extents, checked before expanding installation:

| Native target | RVA | Bytes |
| --- | --- | --- |
| SliderOptionWidget.SetWidgetData | D09C50 | 592 |
| SliderOptionWidget.OnSliderValueChanged | D0A1E0 | 117 |
| SliderOptionWidget.OnAboutToReleaseContext | D09EA0 | 288 |
| NavigationLOD.UpdateLOD | F8ECF0 | 75 |
| NavigationFleetWidget.OnDidBindContext | F7C870 | 335 |
| NavigationFleetWidget.OnAboutToReleaseContext | F7CEA0 | 283 |
| NavigationFleetWidget.OnEnable | F7D8B0 | 344 |
| NavigationFleetWidget.OnDisable | F7DAF0 | 236 |
| MessageBox.Show(context) | 70B5F0 | 81 |
| MessageBox.Show(context, callback) | 70B650 | 257 |
| TextOptionWidget.SetWidgetData | D0A470 | 293 |
| TextOptionWidget.ClearWidgetData | D0A680 | 271 |
| Selectable.DoStateTransition | 47A9650 | 805 |

These exceed the bundled x64 SPUD 24-byte overwrite. Runtime also rejects tiny
or interior entries using unwind metadata. Client SHA256:
`487af4bb9c697c353be9714359a97dddcece5dab872622a6c498a27bbfc44f40`.
This is Windows evidence, not proof of macOS hook fit or native widget behavior.

## Camera and previews

Keyboard zoom speed offers 0–1000 in steps of 25 with whole-number labels. Pan
glide offers 0–0.99 in steps of 0.01; it retains the existing pan formula. These
are UI editing ranges, not new TOML constraints. Out-of-range loaded values are
preserved and explained rather than clamped. The shared slider path controls
display precision without writing a loaded value.

Preview toggles and their existing hotkeys call the same owner, so live state,
readback and saving agree. Locate/Recall labels invert their stored disable
flags. Cargo targets are visible only while auto-open is ON, with their saved
preferences retained while hidden. Hook installation success gates each group.

## Future organization and commands (design notes)

Use player tasks for navigation and TOML sections as storage references.
Introduce a group only when it gains a working control and explicit apply path.
Changing placement must not change storage identity. Confirmations continue on
the native confirmation page; arbitrary TOML keys are not discovered as controls.

A future **Restart client** command could support controls that explicitly need
restart. It would perform an ordinary client restart, settle pending saves using
the existing lifecycle, and relaunch through a supported lifecycle owner. Cache
clearing is a separate operation and must not be called by this command. This is
an idea only: the current branch adds neither restart-only controls nor a restart
command. The ownership/relaunch details need their own design before implementation.

Hotkey editing follows the first real selection and persistence checks. Reuse the
current parser and binding map; add an explicit capture mode with Escape to cancel,
conflict feedback and a deliberate unbind action. Gameplay shortcuts must not fire
while a chord is being captured. Do not serialize display labels as key identities.

## Runtime gate

Before promoting the Navigation slice, verify all three choices, Alt+I changes
while visible, Back/reopen, restart persistence, and an external TOML edit conflict.
Bind build receipts to the installed artifact. The existing synthetic navigation
probe is not evidence that a new selection widget or real persistence path works.
