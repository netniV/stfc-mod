#include "patches/fleet_opc_sample.h"
#include "patches/miner_opc_tracker.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

// Each scenario models a reachable observation sequence; compile-time checks also run in release builds.
constexpr bool crossing_and_rearm()
{
  MinerOpcTracker tracker;
  if (tracker.Observe(1, true, true, false, false))
    return false;
  if (!tracker.Observe(1, true, true, true, true))
    return false;
  if (tracker.Observe(1, true, true, true, true))
    return false;
  if (tracker.Observe(1, true, true, false, true))
    return false;
  return tracker.Observe(1, true, true, true, true);
}
constexpr bool quiet_baselines()
{
  MinerOpcTracker tracker;
  if (tracker.Observe(1, true, true, true, true))
    return false; // Already OPC at first sight.
  tracker.Observe(1, true, true, false, true);
  if (tracker.Observe(1, true, true, true, false))
    return false; // Reseeding.
  if (tracker.Observe(1, true, true, true, true))
    return false;
  tracker.Observe(1, true, true, false, true);
  return !tracker.Observe(2, true, true, true, true); // Slot replacement.
}
constexpr bool interrupted_mining()
{
  MinerOpcTracker tracker;
  tracker.Observe(1, true, true, false, true);
  tracker.Observe(1, false, false, false, true);
  if (tracker.Observe(1, true, true, true, true))
    return false;
  tracker.Observe(1, true, true, false, true);
  tracker.Observe(1, true, false, false, true); // Missing/non-finite cargo breaks continuity.
  return !tracker.Observe(1, true, true, true, true);
}
static_assert(crossing_and_rearm());
static_assert(quiet_baselines());
static_assert(interrupted_mining());
int main()
{
  // A cargo event within the TTL must not redraw the old protected state.
  FleetOpcSampleCache event_cache;
  int                 event_reads = 0;
  bool                source_opc  = false;
  auto                event_read  = [&] {
    ++event_reads;
    return FleetOpcCargo{true, source_opc, source_opc ? 2.0 : 0.0, 1.0};
  };
  assert(!event_cache.Read(0, 1, 100, 4, 1000, event_read).opc);
  source_opc = true;
  event_cache.Invalidate(0);
  assert(event_cache.Read(0, 1, 100, 4, 1500, event_read).opc); // Cargo-event highlight sees fresh value.
  assert(event_cache.Read(0, 1, 100, 4, 1501, event_read).opc); // Timer/alert reuse the new value.
  assert(event_reads == 2);
  event_cache.Invalidate(-1);
  event_cache.Invalidate(10);
  assert(event_cache.Read(0, 1, 100, 4, 1502, event_read).opc);
  assert(event_reads == 2);

  FleetOpcSampleCache cache;
  int                 reads = 0;
  auto                read  = [&] {
    ++reads;
    return FleetOpcCargo{true, reads > 1, static_cast<double>(reads), 1.0};
  };
  assert(!cache.Read(0, 1, 100, 4, 1000, read).opc); // Timer populates sample.
  assert(!cache.Read(0, 1, 100, 4, 1200, read).opc); // Highlight reuses it.
  assert(!cache.Read(0, 1, 100, 4, 1999, read).opc); // Alert reuses it.
  assert(reads == 1);
  assert(cache.Read(0, 1, 100, 4, 2000, read).opc); // Hidden overlay: alert refreshes expired sample.
  assert(reads == 2);
  cache.Read(0, 2, 100, 4, 2001, read); // New fleet.
  cache.Read(0, 2, 200, 4, 2002, read); // New managed object.
  cache.Read(0, 2, 200, 1, 2003, read); // New state.
  assert(reads == 5);
  cache.Read(1, 2, 200, 1, 2003, read); // Independent slot.
  assert(reads == 6);
  assert(!cache.Read(-1, 2, 200, 1, 2003, read).known);
  assert(!cache.Read(10, 2, 200, 1, 2003, read).known);
  assert(reads == 6);
  auto unknown = [&] {
    ++reads;
    return FleetOpcCargo{};
  };
  assert(!cache.Read(0, 2, 200, 1, 3003, unknown).known);
  assert(!cache.Read(0, 2, 200, 1, 3004, read).known); // Failed reads share the same bounded retry interval.
  assert(reads == 7);
}
