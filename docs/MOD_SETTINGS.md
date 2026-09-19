# Mod settings: current architecture and behavior

This is the current contract for the expanded Windows x64 settings UI. The
[foundation notes](MOD_SETTINGS_FOUNDATION.md) describe the first FC-only slice;
their prototype counts and proposed budgets are historical, not current limits.

## Ownership

Feature adapters own live values, availability and persistence. A
`ValueSetting<T>` owns snapshot validation, guarded application and readback.
`PageCatalog` owns placement and presentation callbacks, without Unity objects
or a second copy of configuration. Registration freezes before native creation.
Each game settings context gets a fresh tree from that immutable plan.

The [native adapter map](MOD_SETTINGS_NATIVE_ADAPTER.md) identifies each hook
owner. Interop, value widgets, action widgets, navigation and styling are separate
concerns. Existing XMake source discovery builds them. Each detour has one owner.
The historical `ModConfirmationSettings` debug patch key remains compatible;
its C++ name is `installNativeSettings`.

## Placement and summaries

Player tasks determine labels and navigation; TOML sections remain the storage
reference. Moving a page does not rename stored keys or change defaults.

| Mod Settings page | Contents | Storage reference |
| --- | --- | --- |
| Camera | Keyboard zoom speed, pan glide | `[graphics]` |
| Fleet Labels | Collapsible Player and Non-player profiles | `[graphics]` |
| Map & Travel | Instant warp mode, shared with its shortcut | `[ui]` |
| Previews & Cargo | Preview shortcuts and automatic cargo previews | `[ui]` |

Empty groups are omitted. Camera and preview controls require their existing
consumer hooks to have installed successfully. FC and Forbidden Tech remain in
the game's native confirmation page. Fleet headings start collapsed and summarize
their current mode, with a two-decimal threshold where relevant. The warp page
row shows its mode. Summaries refresh on binding and existing setting notifications.
Cargo target rows appear only while Automatically open cargo is ON; hiding them
preserves each target preference.

## Honest state and persistence

Readback proves the live value, not file or cloud durability. Unknown state never
looks OFF. Failed application preserves authoritative readback and never issues
an automatic reverse write. Native indicators are suppressed when unknown.

A finite loaded value outside a slider's UI range shows `Out of range; edit TOML`.
A non-finite value shows `Invalid value; edit TOML`. Neither is clamped, saved or
fixed by reopening. TOML is loaded at startup: a manual correction takes effect
after restarting. Ordinary unavailable readers retain `Reopen to retry`.
Disabled sliders receive a feature-owned reason; only Fleet Labels says
`Select Threshold`. Keep suffixes short: the native row truncates long labels.
The shared slider widget rounds display values, including during dragging;
speed uses whole numbers and fractional controls use at most two decimals.
Rendering never rounds or saves a loaded preference.

The existing single writer serializes TOML edits and coalesces pending changes
per key. It preserves unrelated source and detects conflicting external edits.
Runtime failure/conflict leaves the live edit active. A failure-only notice at
the top of Mod Settings pages says `Active this session; couldn't save. See mod log.`
It concerns mod TOML saves, not the native FC cloud preference. Detailed key and
failure information stays in the log. No success notices or modal dialogs appear.

Failure state belongs to the writer's per-key records. Saving one key cannot
hide another key's failure. A later successful save of the failed key clears it.
Rejected submissions outside the writer leave a conservative session warning,
because they have no tracked completion. The existing runtime callback observes
aggregate status without taking the writer lock; native UI work happens only
when it changes, on the game thread. Opening settings never retries or writes.
F10's 500 ms best effort force close and the ordinary quit/drain path are
unchanged. See [persistence contracts](config-save.md).

## Native views

Widgets keep weak ownership records and restore text, tint, sprites, button
visibility and interactability before reuse. Callback identity and the currently
bound context gate commands. Value rows defer list rebinding until their
request/readback scope finishes. Headings and action rows do not persist
presentation state. The save notice uses the native button-row adapter with its
button hidden and invocation disabled; visibility is checked on refresh.
An unavailable action-widget family leaves controls usable and save details in
the log. Failure notices never create otherwise-empty groups.

There is no new polling hook, save worker, timer or global localization hook.
Platform guards and native method extent checks remain part of installation;
macOS builds do not install these native UI hooks.

## Validation and follow-up

Run `tests/run-settings.ps1`, `tests/run-config-save.ps1` and the Windows build.
Fixtures cover guarded values, range preservation, conditional sections, command
identities, writer failures and shutdown. Native checks separately cover Back,
folding, conditional rows, notice layout and pooled stock-row restoration.
Source observations and their limits are recorded in [the delivery notes](MOD_SETTINGS_POLISH.md).

Shortcut editing, automatic action discovery and optional settings timing scopes
follow in a separate PR. Numeric input boxes and a real client restart command
remain later work. Neither generic TOML editing nor exposing every config key is
implied by this catalog.
