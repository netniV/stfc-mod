# Thin Queue Protection

Restores the active guard from `v2.1.0-guffa.10` alongside Faster Queue Recovery.
This is separate from the retired Kir'shara combat-completion repair.

```toml
[control]
thin_queue_protection = true
```

Enabled by default on Windows x64. Also respects `control.queue_enabled`. The old
`advanced.queue.thin_queue_protection` value is used when the new control key is
absent, preserving an explicit opt-out. Hook installation requires a restart;
enabling a setting later does not install missing hooks.

The native planner (`DoPlanPathAndEngageTarget`) and watchdog (`HandleStall`) run
first. If they removed an exact prefix of targets, left a nonempty unchanged
suffix, and left the same fleet idle without an active engagement, the guard
rechecks that state and asks the native `TryPlanPathAndEngageTarget` to resume.
Pending/last-target latches must be absent or refer only to removed targets.
Reordering, replacement, truncation, or a latch naming a surviving target rejects
recovery. An unchanged queue is never enough evidence to retry.

On disposal, a fleet must be explicitly destroyed with removal reason `Destroyed`
(1). If native handling leaves that target at an inactive queue head, the guard
rechecks it and calls native `ProcessQueue(target, false)`. It does not force
immediate selection, remove arbitrary targets, or edit queue/engagement state.

Faster Queue Recovery still owns the failed-course-response path. The restored
guard uses three different detours and calls the existing native engage entry,
so Faster Queue Recovery can observe those requests normally. The nested planner
and watchdog paths cannot replay a successfully engaged target because the outer
guard sees the resulting active engagement. Queue identity is checked again after
native calls. All three guard hooks must install before any guard action is enabled.

Methods, field offsets/types and enum return representations are checked before
installation; incompatible layouts keep native behavior. This port currently
installs only on Windows x64. macOS runtime/ABI validation is not claimed.

Successful actions log `[ThinQueueProtection] resume` or
`[ThinQueueProtection] process-destroyed-head`; startup logs `ready=true` or an
unavailable warning. There is no per-frame scan or background polling.

Run `tests/run-action-queue.ps1` for both recovery-policy suites. The restored
release tests cover destroyed-head filtering, prefix removal, multiple removed
targets, latch safety, queue replacement/reordering and postcondition changes.
They do not establish game ABI or live wave behavior. Runtime verification still
requires a new build to be deployed and a wave test after restart.
