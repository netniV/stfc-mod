#pragma once

#include <cstdint>

// Value-only state: unavailable cargo breaks continuity instead of inventing a crossing.
struct MinerOpcTracker {
  uint64_t fleet_id = 0;
  bool     mining   = false;
  bool     known    = false;
  bool     opc      = false;

  constexpr bool Observe(uint64_t id, bool is_mining, bool cargo_known, bool is_opc, bool publish)
  {
    const bool crossed = publish && fleet_id == id && mining && is_mining && known && cargo_known && !opc && is_opc;
    fleet_id           = id;
    mining             = is_mining;
    known              = is_mining && cargo_known;
    opc                = known && is_opc;
    return crossed;
  }
};
