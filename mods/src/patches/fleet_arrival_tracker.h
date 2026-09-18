#pragma once

#include <cstdint>

// Only the phases relevant to identifying one arrival per observed journey.
enum class FleetArrivalPhase { Other, Charging, Warping, Impulsing };

struct FleetArrivalTracker {
  uint64_t fleet_id = 0;
  uint64_t epoch = 0;
  bool journey_observed = false;

  bool Observe(uint64_t id, uint64_t observation_epoch, FleetArrivalPhase before,
               FleetArrivalPhase after, bool native_previous_warping)
  {
    if (fleet_id != id || epoch != observation_epoch) {
      fleet_id = id;
      epoch = observation_epoch;
      journey_observed = false;
    }
    if (before == FleetArrivalPhase::Warping || before == FleetArrivalPhase::Charging
        || after == FleetArrivalPhase::Charging
        || after == FleetArrivalPhase::Warping)
      journey_observed = true;

    // A sampled intermediate state can hide Warping -> Impulsing even though
    // native history still records it. Require an observed journey; native
    // history alone can be stale after startup or baseline reseeding.
    const bool arrival = after == FleetArrivalPhase::Impulsing
                         && (before == FleetArrivalPhase::Warping
                             || native_previous_warping);
    if (!arrival || !journey_observed)
      return false;
    journey_observed = false;
    return true;
  }
};
