#pragma once

#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace gamestate_export
{
  void init();
  void export_now();  // Force immediate full export

  // Export functions
  void export_gamestate();     // Full snapshot export (all per-section files)

  // Data capture functions called from sync hooks
  void capture_player_data(const nlohmann::json& data);
  void capture_player_alliance(const std::string& alliance_name, const std::string& alliance_tag);
  void capture_resources(const nlohmann::json& data);
  void capture_buildings(const nlohmann::json& data);
  void capture_ships(const nlohmann::json& data);
  void capture_research(int64_t id, int32_t level);
  void capture_officers(uint64_t id, int32_t rank, int32_t level, int32_t shards);
  void capture_officer_trait(uint64_t officer_id, uint64_t trait_id, int32_t level);

  // Peace shield: active_expiry_epoch = 0 means no active shield.
  // Pass token_count = -1 to update expiry only (from StarbaseDetailedScan path).
  // Pass active_expiry_epoch = -1 to update token count only (from inventory path).
  void capture_peace_shield(int64_t active_expiry_epoch, int64_t token_count);

  // Drydock assignments: map of drydock_id (1-based) -> player ship id
  void capture_drydock_assignments(const std::vector<std::pair<int32_t, int64_t>>& assignments);

  // Missions
  void capture_missions_active(const std::vector<std::pair<int64_t, int64_t>>& missions); // {instance_id, mission_id}
  void capture_missions_completed(const std::vector<int64_t>& mission_ids);
} // namespace gamestate_export
