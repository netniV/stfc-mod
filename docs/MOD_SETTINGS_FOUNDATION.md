# Boolean settings foundation and native FC control

Historical foundation slice: see [the current settings contract](MOD_SETTINGS.md)
and [native adapter ownership](MOD_SETTINGS_NATIVE_ADAPTER.md) for the expanded
system. Prototype counts and proposed budgets below describe this first slice.

The controller and Fleet Commander preference adapter back a Windows x64 native
confirmation-page control. Mod-owned TOML
persistence and the Community Mod category are separate work.

`settings/boolean_settings.h` is independent of Unity and storage. Definitions have
a stable ID, readable label, read callback, and immediate-write callback. Registry
IDs are unique and registration freezes on first lookup. All operations belong to
the constructing thread; there is no polling, background work or disk I/O.

Consumers must keep unknown state separate from a boolean. `ReadResult` carries
availability, an optional positive value, and an adapter generation. A snapshot
also carries controller identity, revision and lifecycle epoch. Passing a stale
or foreign snapshot rejects the request. `RenderScope` suppresses user-write
handling around native binding/refresh callbacks. Reentrant application is Busy.

Writes re-read before applying, skip already-satisfied values and verify readback.
Failed or uncertain writes never trigger an automatic reverse write. The returned
snapshot contains a fresh authoritative read when available; `Unverified` must
not be rendered as successful application. `AppliedVerified` is local verification,
not a claim of cloud durability.

## Fleet Commander adapter

`FleetCommanderConfirmationSetting()` provides the native UI setting. ON means show confirmation. Reads use the existing
PersistentPrefsManager's `GetBool(key, false, false)`: the final false prevents
insertion of a missing preference while preserving the game's default. Writes use
the native FC setter. The adapter never instantiates managers, invokes abilities,
forces cloud saves, or enumerates other preferences.

Metadata is resolved lazily and checked before access. Two weak handles detect
replacement of the preference manager or saved-data object without retaining
account data. Unavailability invalidates the observed generation. One process-wide
root holds the constant, non-sensitive preference key; no work runs while idle.

The UI calls `InvalidateFleetCommanderConfirmationSession()` before the native
preference manager's RegisterEvents (initialization/reload), session-start handler,
and cloud-load entry. It immediately invalidates live view snapshots. These are
substantive functions; neither the tiny OnApplicationReload wrapper nor the
LifecycleUpdatedEventHandler save-timer path is hooked. Exact-client account
transition validation is still required; object identity alone is insufficient.

The prototype recovery shortcut has been removed. Use the native settings row;
legacy `enable_fc_ability_confirmation` entries are no longer consumed.

## Standalone controller tests

Windows (clang++ with the installed C++ toolchain):

```powershell
clang++ -std=c++23 -Wall -Wextra -Werror -I mods/src tests/boolean_settings_test.cc -o boolean_settings_test.exe
./boolean_settings_test.exe
```

On Unix, use the equivalent compiler invocation with `-pthread`. Keep executables
outside tracked source. These tests cover the pure state machine, not native ABI,
account lifecycle wiring, frame timings, or cloud persistence.

Before extending the UI, measure the baseline and candidate with the same scene,
FPS cap and diagnostics: no scheduled closed-menu work or per-frame allocations;
initial target <=1 ms added normal bind/refresh work at p95, <=2 ms per normal
operation. These are proposed UI acceptance budgets, not measured P1 results.

## Native UI adapter (P2 candidate)

The first registered control is `[MOD] Confirm Fleet Commander abilities`, under
the existing confirmation category. ON means show confirmations; OFF means skip.
The adapter is independent of mod hotkeys and does not install a global localization
hook. It overrides TextLocalizer after native binding and clears its own overrides
on release/rebind, using weak ownership records rather than matching visible text.

`BooleanView` retains the displayed snapshot. Rendering suppresses writes; stale
clicks conflict; an uncertain apply remains unresolved until a subsequent bind.
Rejected writes with known readback retain that value and show a retry message.
Unknown values suppress both native switch/state visual nodes while retaining the
label. The prefab must prove that those nodes are descendants of the row and do
not contain the label; otherwise that UI is unsupported. Exact visual validation
of this behavior remains a release gate.

Eight weak view records bound bookkeeping. Native contexts own rows/delegates;
there are no strong roots retaining historical settings pages. Native release
clears records, with dead-record reclamation on binding as a fallback. A successful
write refreshes other live framework views. No polling or file work is scheduled.

Each callback registration owns a permanent MethodInfo copy with replaced direct,
virtual and runtime-invoker pointers. Matching native schema supplies reflection
metadata only; the donor MethodInfo remains untouched. Closed delegates must point
to that owned descriptor. The native setter delegate is deliberately inert:
only a live widget's explicit change handler can submit its displayed snapshot.
Reflection and refresh callbacks cannot authorize writes. This does not claim a
general managed-method registration API.

All seven hook bodies are preflighted for signatures, distinct addresses, exact
Windows unwind-table entries and at least 64 bytes of native extent. Hooks remain
inert until installation completes. Other platforms omit this control; support
awaits their own native extent and runtime evidence.

Additional standalone tests:

```powershell
clang++ -std=c++23 -Wall -Wextra -Werror -I mods/src tests/boolean_view_test.cc -o boolean_view_test.exe
./boolean_view_test.exe
clang++ -std=c++23 -Wall -Wextra -Werror -Wno-unused-parameter -I mods/src -I third_party/libil2cpp tests/native_boolean_callback_test.cc -o native_boolean_callback_test.exe
./native_boolean_callback_test.exe
```

These cover view failure transitions and owned native callback invocation pointers.
They do not establish delegate construction, DynamicInvoke, Unity pooling, unknown
prefab presentation, account transitions, cloud durability or frame-time budgets
on a running game. Those require the exact candidate artifact, not the earlier
play prototype's successful tests.
