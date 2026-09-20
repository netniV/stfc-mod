# Shortcut editor: port evidence

The editor ports preserved source `3d41a3e9` onto the refreshed controls and
keyboard-layout parents. Baseline `8a97b634` merges controls #282 (`86552e6b`)
and keyboard layout #269 (`24b9a538`); compare against that baseline to review
the editor alone. The upstream dev diff also includes the unmerged parents.

## Extraction boundaries

The port takes the editor, draft/capture models, action presentation metadata,
MapKey replacement/overlap helpers, page-owned cancellation and optional timing.
Config loading registers each canonical action/default; the runtime writer
registers those shortcut keys. The existing ScreenManager hook suppresses game
shortcut dispatch while capture owns input; no additional detour is installed.
Six fork-only console presentation entries are omitted because those actions
are absent upstream. No console, OPC, audio or play integration is included.

Extraction review found one dependency-failure correction: resolve Unity's focus
query before exposing the editor. If unavailable, registration returns before
adding its update callback or pages, so recording cannot acquire input ownership.
The capture state machine still requires a focused key release after Alt-Tab.
A stale test comment about live replacement was also corrected.

The parent writer, TOML editor and persistence fixtures are retained. The #269
diagnostic-save exception guard and checked startup save remain intact. Both
settings runners include the shortcut fixture, including the POSIX runner that
was missing it in the preserved source. Stored keys/defaults are unchanged;
there is no new user configuration option requiring localized examples.

## Reused source observations

The reviewed source polish used the AX-deployed Windows x64 artifact from
`802b1b18`, DLL SHA-256
`4BF575617E6FE780D8F3524CB48D2D385B9AEAA6CCE6FB65A23D5B52EAFF4075`,
client GameAssembly SHA-256
`487AF4BB9C697C353BE9714359A97DDDCECE5DAB872622A6C498A27BBFC44F40`.

The user confirmed draft survival through scrolling/folding, cancellation on
Back, Shift held across Alt-Tab without gameplay dispatch, and fresh Escape
cancellation. Restore staging/cancel and overlap inspection with Next passed.
The overlap example included a fork-only action; it is evidence of traversal,
not a prediction of the action names on this upstream feature set.
Uncategorized discovery has fixture coverage; no populated native fallback
demonstration is claimed. Full default publication is covered by draft fixtures.

Opt-in source measurements observed tree construction (2 samples, mean 12.30 ms,
max 13.01 ms), page binding (119 samples, mean 3.60 ms, max 10.10 ms), and action
refresh (45 samples, mean 3.54 ms, max 9.69 ms). These include nested work on one
client, with no baseline or p95 estimate. They do not justify a snapshot cache
or predict timings for the extracted feature set.

## Port checks

Run seven settings fixtures, the persistence suite, keyboard layout/chord/
dispatch fixtures, Windows build and `git diff --check`. CI must also build both
macOS architectures; those builds exercise the shared fixtures and synthetic
Mach-O extent gates. Native shortcut support is enabled on macOS in code and is
gated at install by resolved-metadata extent checks, but the in-game macOS ARM
runtime smoke (page rendering, capture ownership, Command capture and dispatch,
restart persistence) remains unverified evidence.
Review the extraction and narrow integration changes against the exact PR head,
reusing the completed parent and source reviews.

Source observations are not a smoke pass for a newly built upstream artifact.
Exact-port native smoke and dependency landing remain separate gates. Record
the tested build/deployed hash when collecting that smoke check.
