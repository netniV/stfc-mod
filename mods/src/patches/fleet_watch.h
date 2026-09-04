#pragma once

#include <prime/FleetPlayerData.h>

#include <cstdint>

namespace fleet_watch
{
enum class Source : uint8_t {
  FleetManager,
  FleetStateWidget,
};

struct Snapshot {
  int        slot     = -1;
  uint64_t   fleet_id = 0;
  FleetState state    = FleetState::Unknown;
};

struct Transition {
  Snapshot         before;
  Snapshot         after;
  FleetPlayerData* fleet  = nullptr;
  Source           source = Source::FleetManager;
};

using TransitionCallback = void (*)(const Transition& transition);
using FastPollPredicate  = bool (*)(FleetState state);

struct Subscription {
  TransitionCallback on_transition   = nullptr;
  FastPollPredicate  needs_fast_poll = nullptr;
};

// Registers a process-lifetime fleet-state observer. The first subscription installs the shared observation hooks;
// with no subscriptions, Fleet Watch performs no hooks, scans, or IL2CPP work.
// Register during patch installation. Callbacks run synchronously on the game thread, and Transition::fleet is valid
// only for the duration of the callback.
bool Subscribe(Subscription subscription);

// Called by the shared ScreenManager.Update detour. Consumers should use Subscribe rather than calling this directly.
void Tick();
} // namespace fleet_watch
