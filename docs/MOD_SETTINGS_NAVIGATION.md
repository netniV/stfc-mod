# Rebuildable mod settings pages

This describes the navigation contracts. The current
control placement, conditional rows, summaries and native adapter ownership are
documented in [Mod Settings](MOD_SETTINGS.md).

This foundation separates presentation placement from a setting's owner. The
intended native path is Settings > Mod Settings > group > setting. Current page membership follows the task layout in `MOD_SETTINGS.md`. Moving a
control must not rename its stored setting or introduce another copy of its value. Confirmation controls
continue to belong on the native confirmation page.

`PageCatalog` holds stable page IDs, labels, parent IDs and references to existing
`BooleanSetting`, `ChoiceSetting`, `SliderSetting` and `ActionSetting` instances, plus static
headings, in registration order. Multiple independent choices can share a page.
Parents register first; invalid parents, duplicate
pages and conflicting setting owners are rejected. The same setting can appear
on different pages, with the same authoritative read/write adapter. Registration
freezes at the first build. Definitions and setting owners outlive their views.

The catalog builds a parent-first plan once during installation; each new native
settings context receives fresh managed pages from that plan. Empty branches are omitted,
including an empty root. Building a plan neither reads nor writes settings and
retains no Unity objects. Views reuse `BooleanView` for guarded rendering, stale
request rejection and authoritative readback. A released view cannot authorize
another write. Rebuilding reads current state when each new view binds.

The native adapter must create fresh managed contexts from this plan, avoid
duplicate roots within one context, and release any temporary roots on failure.
Pooled widgets must clear owned label/state overrides before reuse. No setting
registration may install an additional copy of an existing widget detour.

The shared native adapter supports Windows x64 and macOS and creates boolean,
selection, slider and action rows through validated managed builders. It restores
owned text overrides on category unbind/rebind and page destruction, using scoped
human-text overrides without a global localization hook. Optional category/page,
heading and value hooks install only when their registered controls need them.

Historical build261 measurements covered four category/page lifecycle methods.
Current Windows client270 static measurements cover those methods and the heading,
action, selection and slider families; every selected SPUD overwrite window fits
its method extent. Those disk measurements do not establish live relocation,
callback lifetime or native presentation. Exact artifact navigation/pooling smoke
and supported Mac native extent/execution evidence remain qualification gates.

Register through `ModPages()` before settings installation. Current pages are
Camera, Fleet Labels, Map & Travel, Previews & Cargo, and Shortcuts, as documented in
[Mod Settings](MOD_SETTINGS.md). Instant warp shares Alt+I's owner and persistence.
Fleet Labels places Player and Non-player sections on one page, each with detail
choices and a percentage slider. Native text-only headings use scoped labels and
row tints cleared on refresh/clear. Native confirmation placement stays unchanged.
Selection controls share the typed setting/view guards with booleans and retain
the whole integer value in each row snapshot. Three selection-widget hooks have
verified Windows x64 extents of 146, 355 and 281 bytes. Selection prefabs may put
their toggle on the row itself: an unavailable selection clears its selected
index and disables interaction instead of hiding its label's container.

The native adapter shares the `ModConfirmationSettings` registry entry,
controlled by default-enabled `[patches].nativesettingshooks` in all builds.
Disabling it skips native UI installation. Stable weak-view storage is sized once
from all registered control rows plus the native confirmation rows, retaining a
minimum of eight slots. There is no fixed maximum of eight rows. Native contexts
own rows and delegates; headings use scoped text records. Heading-only pages are
pruned as empty.

Persistence stays with explicit feature adapters, independently installed through
`[patches].runtimeconfighooks`. Live changes and their asynchronous save results
are distinct; page construction never writes TOML. One worker registers mode,
fleet-label and FT keys and retains pending changes per setting.

Run `tests/run-settings.ps1` on Windows or `bash tests/run-settings.sh` on macOS.
The catalog fixture covers repeated builds, empty branches, registration failures,
shared setting identity, existing BooleanView readback/unbind semantics and UI-thread
ownership. The same runners retain the original boolean/view/callback fixtures.
