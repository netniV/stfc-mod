# Native settings selection appearance

The build261 `gamesettings` Pre-Bundle contains a `LanguageOption` prefab used
by `SelectionItemOptionWidget`. Its direct children include `BG`, `Label`,
`CheckBox`, and `Arrow`. The widget's Toggle and Animator live on the row itself.
Toggle.graphic is null; the Animator controls the actual checkmark.

`ImageToggleOn` and `ImageToggleOff` animate three distinct parts:

| Part | On | Off |
| --- | --- | --- |
| Arrow | Active | Inactive |
| BG.sprite | `SelectedBG_raw` | Native normal background |
| Label font color | Dark | White |

The clips also animate BG geometry. Both contain no animation events. These
findings came from read-only prefab/clip inspection, not runtime animation sampling.
The controller's selection state is independent of its pointer Button layer.

The normal BG sprite is shared with the native category, text and slider rows.
Category rows use a direct `Background` child; text/slider/selection rows use `BG`.
This explains why looking for Image on the widget root did not tint the backgrounds.

For owned choices, override the drawn BG sprite while retaining native selection
animation, geometry, and checkmark activity. Collect the normal and selected
sprites from existing settings widgets using weak handles. If a sprite is not
available yet, leave the native presentation for that state until a bind/input
event supplies it. No bundles are loaded, cloned, modified, or retained by this
adapter. Rich text keeps labels light at rest, dark while pressed, and bold for
the selected value. The checkmark tint follows that contrast treatment.

Cleanup is guarded against synchronous reentry. Re-enabling a disabled Toggle
calls `Selectable.OnSetProperty` and then `DoStateTransition` immediately; the
transition must not reapply appearance while the row is being detached. All
overrides are restored before releasing widget references. An isolated fixture
compiling the actual `Clear` body reproduced this callback ordering defect before
the guard and passed afterward, including recursive clear. Its native callback
was simulated; pooled-widget behavior still needs the in-game check.

Pointer feedback uses the existing native input-state transition event. The new
Windows x64 `Selectable.DoStateTransition` hook has an 805-byte PE unwind extent
at RVA `0x47A9650`, exceeding SPUD's 24-byte overwrite. No other mod hook owns
this target. It immediately passes through other control classes and only styles
selection rows tracked by the mod. It does not run through an Update hook.

Client SHA256:
`487af4bb9c697c353be9714359a97dddcece5dab872622a6c498a27bbfc44f40`.
The native UI remains omitted on other platforms; Windows hook evidence is not
proof of macOS hook fit. Runtime checks must verify release/drag-out behavior,
shortcut readback, selected checkmarks, and restoration of pooled native widgets.

## Collapsible section follow-up

The user accepted the appearance candidate `ac006f5f`; its cleanup correction is
preserved in `65dede72`. Collapsing sections is a separate follow-up to that baseline.

Read-only build261 disassembly of `GameSettingsViewController.OnCategorySelected`
(`0xD067E0`) shows it sets `SettingsContext.SelectedOption`, retrieves the selected
container's `Children`, and binds the option panel with a null provider and that
IList. `OptionTabPanelWidget.OnDidBindContext` (`0xD08630`) clears/rebuilds its native
list and wires category callbacks. Section clicks reuse this panel bind with a
filtered array, keeping SelectedOption and the original children intact. The
existing page-selection detour handles only registered section categories on the
current page. This adds no new detour target.

The category prefab has direct `Background` and `Arrow` children. Its Arrow starts
with identity local rotation. Expanded headings rotate that arrow down; release
restores its native rotation and background color before pooling. Runtime evidence
must still verify independent folding, both sections closed, Back/reopen, arrow
orientation, and restoration of ordinary category rows.

The first section candidate (`464362a0`) rendered correctly but did not fold:
the adapter incorrectly looked up `SetContext`, which this widget does not have.
Builds and catalog tests did not exercise native method resolution. The current
dump identifies the call as `Widget.BindDataContext(IDataContextProvider, object)`,
virtual slot 35. The native caller's vtable offset `0x368` matches slot 35 for
this client. Resolve that slot from the non-generic Widget schema through
`il2cpp_object_get_virtual_method`; a name/count-only lookup would be ambiguous
with the typed `Widget<IList>.BindDataContext` overload (slot 39).

The user confirmed expand/collapse works on `8e3a166b` and requested both sections
start collapsed. The next revision initializes visit-local collapsed headings and
applies the same filtered bind after native page selection. Initial folding and
Back/reopen defaults require their own runtime check; the previous result proves
the click path, not the new entry behavior.
