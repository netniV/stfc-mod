#pragma once

#include <array>
#include <cstdint>

struct FleetPlayerData;
enum class FleetState;

struct FleetOpcCargo {
  bool   known           = false;
  bool   opc             = false;
  double current         = 0.0;
  double protected_limit = 0.0;
};

// Game-thread-only, value-only cache shared by the OPC timer, highlight, and alert.
// Object identity is only a cache key, never a retained managed pointer to dereference.
class FleetOpcSampleCache
{
public:
  void Invalidate(int slot)
  {
    if (slot >= 0 && slot < static_cast<int>(entries.size())) {
      entries[slot].valid = false;
    }
  }

  template <class Reader>
  FleetOpcCargo Read(int slot, uint64_t fleet_id, uintptr_t object_id, int state, int64_t now_ms, Reader read)
  {
    if (slot < 0 || slot >= static_cast<int>(entries.size()))
      return {};
    auto& entry = entries[slot];
    if (entry.valid && entry.fleet_id == fleet_id && entry.object_id == object_id && entry.state == state
        && now_ms >= entry.sampled_ms && now_ms - entry.sampled_ms < 1'000) {
      return entry.cargo;
    }
    const auto cargo = read();
    entry            = {true, fleet_id, object_id, state, now_ms, cargo};
    return cargo;
  }

private:
  struct Entry {
    bool          valid      = false;
    uint64_t      fleet_id   = 0;
    uintptr_t     object_id  = 0;
    int           state      = 0;
    int64_t       sampled_ms = 0;
    FleetOpcCargo cargo;
  };
  std::array<Entry, 10> entries{};
};

FleetOpcCargo read_fleet_opc_sample(FleetPlayerData* fleet, int slot, uint64_t fleet_id, FleetState state);

// Call on the existing cargo-change callback before rendering from the shared sample.
void invalidate_fleet_opc_sample(int slot);
