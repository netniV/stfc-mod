# Faster Queue Recovery

When a queued target disappears while a course request is outstanding, the game can remove that target before its failed response arrives. Native failure handling then attempts the removal again. Since it did not remove the front entry this time, it can leave the next target waiting for the watchdog.

Enable the opt-in recovery at startup:

```toml
[control]
faster_queue_recovery = true
```

The feature also respects `control.queue_enabled`. Support is limited to the verified Windows x64 client 261 layout. Other platforms do not install these hooks; incompatible Windows layouts log an unavailable message and retain native behavior.

## Behavior

The adapter records the latest engagement attempt for up to eight fleets, using weak queue identities and full 64-bit target IDs. A failed, non-recall course response may request native planning only when:

- The response belongs to the same queue and latest target/attempt, observed within 30 seconds.
- The queue was engaging on entry to the response and native processing cleared that flag.
- A different front target remains, and the failed target is absent from every inspected queue.
- The native retry decision was false. Ordinary retries are unchanged.

The request record is consumed once. The native planner selects and validates the next target. The mod does not force ship state, clear engagement flags, change retry counts, or remove targets. If another fleet still queues the target, the native cross-fleet removal path remains responsible.

Queue storage is bounded and validated. Unknown layouts/content are ineligible. Expired/replaced requests release their weak handles; native session cleanup clears all records. A native attempt returning skip/stop cancels only its own record, preserving newer reentrant attempts.

There is no watchdog hook, frame scan, timer, background worker, or per-engagement logging. Queue inspection occurs in existing callbacks; all-queue inspection runs only for a potentially eligible failed response. Response matching is not a server-issued request ID, so delayed same-target responses remain an interoperability limitation.

## Native integration

One detour owns each method. Installation checks metadata, exact RVA, PE unwind extent, and a 24-byte prefix before installing any hook. Windows x64 client 261 targets:

| Method | RVA | Native extent |
| --- | --- | --- |
| TryPlanPathAndEngageTarget | `0x1109f60` | 2340 bytes |
| ShouldRetryFailedSetCourse | `0x110d280` | 662 bytes |
| OnSetCourseResponseEventHandler | `0x110d070` | 514 bytes |
| StopWatchdogAndClearAllQueues | `0x110bcc0` | 493 bytes |

The course event is a 24-byte value type, with fleet ID at 0, success/recall at 8/9 and boxed target at 16. Metadata field offsets include the boxed object header. The native retry handler is called synchronously inside the course handler; returning true selects its existing planner branch.

**THIS WAS FIXED BY SCOPELY:** the older off-screen Kir'Shara combat-completion repair is obsolete. Remove that workaround and its `kirshara_queue_repair` setting; it is not a prerequisite for Faster Queue Recovery. This feature addresses only the separate unavailable-target/course-response race described above.

## Validation

Run `tests/run-action-queue.ps1` on Windows, or compile `tests/action_queue.cc` with a C++23 compiler and `-Imods/src`. Tests use the production policy and request store with fake weak handles. They cover one-shot recovery, reordered/still-present targets, cross-fleet rejection, stale attempts, replaced/collected queues, expiry, reentrant cancellation, bounded capacity and cleanup. They do not model game ABI or network scheduling.

The prototype produced two observed handoffs with the next target attempted 1–2 ms later and successful responses within 388–532 ms. Those timings are observations, not a latency guarantee. The final adapter requires its own smoke test: normal queued combat; removal of the outstanding target; removal before the first successful course response; queue clear/rebuild; recall; and session restart. Group-wave behavior needs additional coverage when available.
