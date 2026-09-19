# Rebuildable mod settings pages

This foundation separates presentation placement from a setting's owner. The
intended native path is Settings > Mod Settings > group > setting. Group names
and final membership are deliberately undecided; moving a control must not rename
its stored setting or introduce another copy of its value. Confirmation controls
continue to belong on the native confirmation page.

`PageCatalog` holds stable page IDs, labels, parent IDs and references to existing
`BooleanSetting` instances. Parents register first; invalid parents, duplicate
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

Current build261 metadata exposes both root and parent-taking `AddCategory`
overloads on `SettingsContext`, plus parent-taking toggle/selection builders.
The Windows bridge calls that native builder and adds boolean rows through the
existing confirmation adapter. It restores owned text overrides on category
unbind/rebind and page destruction; titles use the same scoped human-text override
as existing confirmation labels. No global localization hook is installed.
Four substantive category/page lifecycle hooks are installed only when registered
pages exist. Current x64 bodies are 366, 250, 572 and 608 bytes respectively, each
larger than SPUD's 24-byte overwrite. Other platforms omit the native UI pending
their own hook evidence. Metadata/builds alone do not validate presentation or
callback lifetime; repeated navigation/pooling remains a runtime gate.

Register through `ModPages()` before settings installation. The production catalog
is empty: no final group layout, settings placement or new preference is shipped
by this infrastructure slice. This supersedes the earlier General > Community Mod
placement proposal; native confirmation placement remains unchanged.

The current native bridge shares the `ModConfirmationSettings` patch installation
and its debug installation switch. Disabling that patch disables both native UI
surfaces. Settings retain their own identity and persistence independently of it.
The shared native adapter currently supports eight simultaneously bound mod
boolean rows across pages. Plan populated groups within that existing limit;
catalog registration does not itself guarantee native widget capacity.

For a temporary Windows debug navigation fixture, launch with
`STFC_MOD_SETTINGS_NAV_TEST=1`. It builds Mod Settings > Infrastructure Test >
Nested Group and mirrors the existing FC setting owner. It does not create a
second preference. A separate Infrastructure test toggle holds only an in-memory
fixture value, proving that multiple rows use different owners. Leave the FC
switch alone when checking labels, nesting and Back; it writes the real FC
preference if intentionally clicked. The synthetic toggle writes no file. The
environment option is absent from release builds and defaults off. Remove it and
restart to return to the empty production catalog. No data is cleared.
For the read-callback lifecycle check, additionally set
`STFC_MOD_SETTINGS_NAV_REENTRY_TEST=1`. The synthetic reader once releases its
own bookkeeping and rebinds the same native widget. A bounded PASS/FAIL log checks
that the in-flight slot is not reused. This probe does not run for real settings.
That option also adds a second synthetic toggle. Changing the Infrastructure test
toggle once invokes the second setter, which releases and rebinds the first row
while both requests are active. A separate nested-write PASS/FAIL log verifies
that the outer request's slot stays protected. Revisit afterward to check readback.

Persistence stays with explicit feature adapters. A live mod change and its
asynchronous save result are distinct; page construction never calls the TOML
writer. The current writer supports its one known mode setting. This work does
not add arbitrary TOML browsing, a second save worker, automatic config hot reload,
sliders/selection abstractions without a consumer, or speculative profiler options.

Run `tests/run-settings.ps1` on Windows or `bash tests/run-settings.sh` on macOS.
The catalog fixture covers repeated builds, empty branches, registration failures,
shared setting identity, existing BooleanView readback/unbind semantics and UI-thread
ownership. The same runners retain the original boolean/view/callback fixtures.
