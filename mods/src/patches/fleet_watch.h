#pragma once

#include <prime/FleetPlayerData.h>

namespace fleet_watch
{
struct Snapshot {
  int        slot     = -1;
  uint64_t   fleet_id = 0;
  FleetState state    = FleetState::Unknown;
};

struct Transition {
  Snapshot         before;
  Snapshot         after;
  FleetPlayerData* fleet = nullptr;
  // Changes whenever the observer resets its baseline; consumers must discard journey history.
  uint64_t observation_epoch = 0;
};

using TransitionCallback = void (*)(const Transition& transition);
using FastPollPredicate  = bool (*)(FleetState state);
// Every observation, including unchanged states and quiet baseline samples. Never retain fleet.
// allow_notifications is false during baseline/reseeding or for a newly observed fleet.
using ObservationCallback = void (*)(const Snapshot& snapshot, FleetPlayerData* fleet, bool allow_notifications);

struct Subscription {
  TransitionCallback  on_transition   = nullptr;
  FastPollPredicate   needs_fast_poll = nullptr;
  ObservationCallback on_observation  = nullptr;
};

// Registers a process-lifetime fleet-state observer. The first subscription joins the shared screen-update dispatcher;
// with no subscriptions, Fleet Watch performs no scans or IL2CPP work.
// Register during patch installation. Callbacks run synchronously on the game thread, and Transition::fleet is valid
// only for the duration of the callback.
bool Subscribe(Subscription subscription);

// Called by the shared ScreenManager.Update detour. Consumers should use Subscribe rather than calling this directly.
void Tick();

#ifdef _MODDBG
// Enables a raw transition logger when STFC_MOD_FLEET_WATCH_PROBE=1 for exact-artifact runtime validation.
void InstallRuntimeProbe();
#endif
} // namespace fleet_watch
