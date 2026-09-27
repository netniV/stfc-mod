#include "patches/fleet_opc_sample.h"

#include <prime/FleetPlayerData.h>

#include <chrono>
#include <cmath>

namespace
{
FleetOpcSampleCache s_cache;
}

void invalidate_fleet_opc_sample(int slot)
{ s_cache.Invalidate(slot); }

FleetOpcCargo read_fleet_opc_sample(FleetPlayerData* fleet, int slot, uint64_t fleet_id, FleetState state)
{
  if (!fleet)
    return {};
  const auto now_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
          .count();
  return s_cache.Read(slot, fleet_id, reinterpret_cast<uintptr_t>(fleet), static_cast<int>(state), now_ms, [fleet] {
    FleetOpcCargo sample;
    auto*         cargo    = fleet->CargoHoldData;
    auto*         progress = cargo ? cargo->UnprotectedCargoProgress : nullptr;
    if (progress) {
      sample.current         = progress->CurrentValue;
      sample.protected_limit = progress->MinValue;
      sample.known           = std::isfinite(sample.current) && std::isfinite(sample.protected_limit);
      sample.opc             = sample.known && sample.current > sample.protected_limit;
    }
    return sample;
  });
}
