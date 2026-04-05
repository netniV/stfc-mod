#pragma once

#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>

namespace gamestate_export
{
  void init();
  void export_now();  // Force immediate full export

  // Export functions
  void export_gamestate();     // Full snapshot export
  void export_differential();  // Differential export (changes only)

  // Data capture functions called from sync hooks
  void capture_player_data(const nlohmann::json& data);
  void capture_resources(const nlohmann::json& data);
  void capture_buildings(const nlohmann::json& data);
  void capture_ships(const nlohmann::json& data);
  void capture_research(int64_t id, int32_t level);
  void capture_officers(uint64_t id, int32_t rank, int32_t level, int32_t shards);
} // namespace gamestate_export
