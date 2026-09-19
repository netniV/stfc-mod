# Settings controls polish: delivery evidence

This port extends PR #282 using preserved play source `3d41a3e9`: live Camera
and Preview/Cargo controls, shared slider formatting, honest range errors,
feature-owned disabled wording, task-oriented navigation, summaries, adapter
separation and save-failure feedback. The shortcut editor, discovery, page-owned
drafts and timing scopes remain in the next PR.

## Source evidence

The source adapter refactor was reviewed and smoke-tested at `13e68131`. The
polish and failure-recovery corrections were reviewed at `802b1b18`, with
settings and persistence fixtures and a Windows releasedbg build.

Relevant native observations used the AX-deployed Windows x64 artifact from
`802b1b18`, DLL SHA-256
`4BF575617E6FE780D8F3524CB48D2D385B9AEAA6CCE6FB65A23D5B52EAFF4075`,
client GameAssembly SHA-256
`487AF4BB9C697C353BE9714359A97DDDCECE5DAB872622A6C498A27BBFC44F40`:

| Check | Source observation |
| --- | --- |
| Navigation and summaries | Camera, Fleet Labels and Map & Travel revisited; heading summaries confirmed. |
| Conditional rows | Cargo targets hid and reappeared when Automatically open cargo changed OFF/ON. |
| Save failure | An intentional external edit of a camera key caused a writer conflict. The live slider remained usable and the amber notice fit without truncation. |
| Save recovery | A later successful save of the same camera key cleared the notice; zoom speed was retained in TOML. |

Loaded out-of-range messages and preservation have fixture evidence; no native
out-of-range screenshot is claimed. Shared slider formatting was accepted in
earlier source checks while dragging and after reopening Camera/Fleet Labels.
These support the transferred behavior; they are not a smoke pass for a newly
built upstream artifact.

## Port verification

The refreshed #280 writer, editor, failure observer and persistence fixtures are
retained, including its later debounce-fixture timing correction. Only control
key registrations extend the runtime adapter. No shortcut recording, binding
registration, keyboard-layout changes, console, OPC or audio payload is extracted.

Run six settings fixtures, the persistence suite and Windows build; record CI
against the published head. Review extraction boundaries and shared-hook
ownership. Native smoke on the exact port remains separate; preserve artifact
identity when collecting it. Source validation does not establish every pooling
transition or native macOS compatibility.
