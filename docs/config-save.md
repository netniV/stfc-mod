# Startup config saves

`Config::Save` writes complete TOML documents for two startup callers: the initial
default config and the generated runtime snapshot. It keeps `File::MakePath`
routing and the existing generated-file warning. Save errors are logged once by
the caller; startup continues with the in-memory configuration.

`SaveConfigDocument` serializes with toml++, parses the output before touching
disk, exclusively creates a sibling temporary file, checks writing and closing,
and replaces the destination. Startup callers remain synchronous. These
whole-document saves do not merge concurrent setting changes or preserve comments.

Windows uses `ReplaceFileW` to preserve existing permissions and streams, with a
temporary backup for its documented partial-failure cases. A missing destination
falls back to a non-replacing move. The caller's startup existence check is not an
exclusive create transaction. Ordinary failures clean up the temporary file; partial
replacement failures retain recovery files and report their location. The backup
name is the reported temporary path plus `.bak`. Recovery is not automatic.
macOS uses rename after copying the existing permission bits. Extended metadata
and hard-link identity are not preserved by that path. Existing symlinks are
resolved before staging. Replacement requires directory permissions in addition
to any file access checks; it cannot exactly match an in-place overwrite.

Successful close/replacement is not a guarantee against power loss. A forced exit
can leave a temporary file. No automatic stale-file sweep is installed.

Run the isolated Windows fixtures with `tests/run-config-save.ps1` after the
normal AX build has installed toml++; `-TomlInclude` can select another include
directory. Fixtures never access the installed game's files.

On macOS, run `bash tests/run-config-save.sh TOML_INCLUDE_DIR`. Both native macOS
CI jobs run these fixtures after the normal build, including permission-bit and
symlink checks. The failure fixture injects short writes and failed closes at
compile time; it does not install test controls in the mod.

Native behavior references:
- [Windows ReplaceFileW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew)
- [POSIX rename](https://pubs.opengroup.org/onlinepubs/9799919799/functions/rename.html)

## Runtime edits

The instant-warp mode shortcut and native mod controls change live values
immediately, then ask one worker to persist explicitly registered keys. Opening
a settings page does not submit a write. Each key retains its own acknowledged value and at
most one pending request; new submissions replace that key's pending value while
an active save finishes. Registration closes when the worker starts on its first
request. Unregistered keys are rejected. The worker does not read game objects or
call Unity.

A submission can delay its save until a quiet interval has elapsed. Replacing a
pending request restarts that key's interval, without delaying other ready keys.
Normal quit drains accepted requests, including delayed ones; cancellation wakes
the worker and discards pending requests. There is one worker for the file, with
no per-control threads or timers. This branch registers instant warp, Fleet Label
detail modes and thresholds, Camera and Preview/Cargo preferences, and the
Forbidden Tech confirmation bypass. Slider
edits use a 150 ms quiet interval. Each feature owns its key registration,
validation, and choice of delay; shortcut editing follows separately.

The worker reads the current file for each attempt. `TomlEditor` caches a parsed
document only while its source bytes match. It uses toml++ source regions to
replace the selected value, preserving unrelated bytes, comments and line endings.
Missing settings are inserted only when reparsing proves the candidate means
exactly the intended document. Values are typed booleans, strings, signed 64-bit
integers or finite doubles and encoded by toml++; quotes, backslashes and newlines
cannot become new TOML instructions. Newly requested NaN/infinity values are
rejected. Existing unrelated TOML values are preserved. Feature-specific numeric
ranges remain the caller's responsibility.

Each request compares the selected value against the last acknowledged disk value,
including whether it was absent. String quoting/escape spelling is not part of
that semantic comparison. Unrelated external
changes survive. A value already equal to the requested value succeeds without a
write; a different external value reports a conflict. Invalid TOML, unsupported
value types and I/O errors leave the live setting alone and log one message per failed
attempt, without file contents or values. No automatic retry loop is installed.
Each key's acknowledged value advances only after success. To reconcile a
conflict, restore its original disk value or restart to load the file. Requesting
a value already present externally also succeeds without a write when the control
can represent that value. Runtime edits do not rewrite the startup-only generated snapshot.

Failure state is tracked per key: saving B cannot clear a failure for A. A later
successful save of A clears A's failure. Failure to start the worker is tracked
the same way and does not prevent a later submission from trying again. Reporter
callbacks include the section/key, run on the worker, and cannot stop it by
throwing. The game adapter exposes aggregate failure status and one optional
process-lifetime observer, called on the game thread only when that status
changes. It uses the existing update dispatcher even if persistence setup failed.
The adapter conservatively retains failures from submissions it could not track.
Settings UI wording and widgets belong to the consumers, not the writer.

The checked replacement re-reads the source after staging and rejects changed
bytes before commit. This is best-effort conflict detection, not an atomic
compare-and-swap with arbitrary external editors: an external write can still
race the final native replacement. File deletion is an I/O error, not permission
to recreate the user's file from cached content.

Runtime persistence currently requires the verified build261 Windows x64 quit
method (RVA `0x43548c0`, native extent 411 bytes, 24-byte SPUD overwrite, complete
initial instruction fingerprint checked at installation). macOS and unmatched
clients keep the shortcut's existing session-only behavior and log that persistence
is unavailable. The editor/storage fixtures run on all supported build platforms;
they do not establish native game-hook compatibility.

An idle normal quit closes admission and passes the original vote through without
replaying quit. When work is active, normal quit stops admission, drains accepted work, then resumes the game's quit
request after observing native worker termination. Save failures do not prevent
exit. A genuine game veto is respected and is not retried automatically. If the
game vetoes after draining, persistence remains stopped for that session;
subsequent mode shortcuts still affect gameplay but are session-only. A stalled
OS write can delay normal quit; F10 remains the escape path. With pending work,
F10 cancels queued requests and allows the active write up to 500 ms on an
independent native thread before terminating. With no pending/active write it
terminates immediately. No disk operation or wait runs in the key handler.
The existing ScreenManager.Update dispatcher supplies one idle callback; there
is no extra frame detour or per-frame logging. Hook controls have process lifetime;
hot unloading the mod is unsupported.

The fixture runners also cover preserving edits, escaped values, conflicts,
numeric encoding, per-key coalescing, debounce expiry/replacement, failed-save
baselines, failure/recovery status, worker-start retry, draining and cancellation. They use isolated
files and compile-time seams; no test switches or artificial delays ship in the mod.
The Windows adapter fixture executes the production lifecycle functions with
controlled worker/Unity boundaries. Separate child processes exercise real native
force-close calls, including a stalled cancellation caller and the 500 ms wait.
Its 5-second watchdog allows scheduling overhead; this is not a hard real-time
deadline guarantee or evidence that the current game detour fired.
