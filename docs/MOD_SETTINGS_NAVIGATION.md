# Rebuildable mod settings pages

This foundation separates presentation placement from a setting's owner. The
intended native path is Settings > Mod Settings > group > setting. Group names
and final membership are deliberately undecided; moving a control must not rename
its stored setting or introduce another copy of its value. Confirmation controls
continue to belong on the native confirmation page.

`PageCatalog` holds stable page IDs, labels, parent IDs and references to existing
`BooleanSetting`, `ChoiceSetting`, `SliderSetting` and `ActionSetting` adapters. Parents register first; invalid parents, duplicate
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

Register through `ModPages()` before settings installation. The production catalog
is empty: no final group layout, settings placement or new preference is shipped
by this infrastructure slice. This supersedes the earlier General > Community Mod
placement proposal; native confirmation placement remains unchanged.

The native adapter shares the `ModConfirmationSettings` registry entry, controlled
by default-enabled `[patches].nativesettingshooks` in all builds. Disabling it skips
native UI installation. Value records are sized from the registered page plan;
there is no fixed eight-row limit. Managed contexts own rows and delegates, while
weak records track bound views without retaining historical settings pages.

Persistence stays with explicit feature adapters. A live mod change and its
asynchronous save result are distinct; page construction never calls the TOML
writer. The current writer registers only instant-warp mode. Its installation is owned
separately by `[patches].runtimeconfighooks`. Choice, slider and action adapters are
included as navigation groundwork; a populated consumer owns its registrations,
validation and persistence. Page construction does not register arbitrary TOML keys.

Run `tests/run-settings.ps1` on Windows or `bash tests/run-settings.sh` on macOS.
The catalog fixture covers repeated builds, empty branches, registration failures,
shared setting identity, existing BooleanView readback/unbind semantics and UI-thread
ownership. The same runners retain the original boolean/view/callback fixtures.
