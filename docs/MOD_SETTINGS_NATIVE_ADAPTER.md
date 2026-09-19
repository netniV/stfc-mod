# Native settings adapter

The Windows x64 adapter owns native contexts, hook installation and temporary
presentation. Feature adapters own live values and persistence.

## File ownership

| File under `mods/src/` | Responsibility |
| --- | --- |
| `patches/parts/mod_settings.cc` | Install core value/session hooks, then optional navigation. A navigation failure leaves native confirmation controls available. |
| `settings/native/interop.*` | Managed invocation, temporary roots, weak handles, signature/extent checks and bounded list access. No feature state or hook installation. |
| `settings/native/value_widgets.*` | Boolean, choice and slider metadata; live view records; guarded render/write/readback; confirmation placement and session invalidation. Installs value/session hooks. |
| `settings/native/value_widget_record.h` | Private lifetime record shared with value-widget styling. Navigation queries busy state without borrowing these records. |
| `settings/native/page_navigation.*` | Immutable plan, fresh page construction, navigation/Back, conditional sections, folding, summaries and heading hooks. Coordinates optional widget installation. |
| `settings/native/action_widgets.*` | Command identity, indexed presentation rows, visibility, invocation and release guards. Installs button-widget hooks. The first consumer here is a noninteractive save-failure notice. |
| `settings/native/row_style.*` | Scoped text, tint, arrow and selection-sprite overrides; restore native appearance before reuse. No writes or hook installation. |

These are internal adapter modules. Metadata accessors support cross-module
signature and hook-overlap checks while retaining lazy resolution. Page plans
are read-only; records and active flags stay with their owners. Each detour is
installed by exactly one module. XMake's existing `src/**.cc` rule builds them.

## Contracts

- UI-thread ownership and weak-view lifetime rules remain unchanged. Views never
  keep old settings pages or account state alive.
- Requests use the displayed snapshot, verify the owner, apply through it, then
  read back. Rendering never authorizes writes.
- Value refreshes defer list rebinding while a value widget is busy. There is no
  new update callback, polling, save worker or persistence path.
- Startup metadata/extent checks, overlap checks, activation gates and install
  order are preserved. Native support remains Windows x64; other platforms retain
  the no-op entry point.
- Disabled slider wording belongs to the feature. Fleet Labels supplies
  `Select Threshold`; other controls do not inherit that instruction. See
  [the current state contract](MOD_SETTINGS.md).

The entry point and member are `InstallNativeSettings` and
`Config::installNativeSettings`. The debug patch key `ModConfirmationSettings`
remains unchanged. Setting IDs, TOML keys and defaults remain stable while
presentation placement evolves.

## Validation

Run the settings fixtures and Windows build. Compare moved hooks against the
previous implementation, including original-call behavior and installation order.
Native smoke checks cover confirmation rows, folding/Back, choices, slider labels,
conditional cargo rows and notice appearance/recovery. Builds and fixtures do
not establish native pooling behavior or macOS hook compatibility.
