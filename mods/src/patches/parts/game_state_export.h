#pragma once

#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace game_state_export
{
  void init();
  void export_now();  // Force immediate full export

  // Export functions
  void export_game_state();     // Full snapshot export (all per-section files)

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

  // Buff catalog (ShipBonusBuffSpecs, type 51)
  struct BuffSpecEntry {
    int64_t  buff_id;
    int32_t  modifier_code;    // CLIENTMODIFIERTYPE_* enum value
    int32_t  operation;        // BuffOperation enum value
    int64_t  faction_id;       // 0 = not faction-specific
    bool     show_percentage;
    std::vector<double> ranked_values; // per-level bonus values
  };
  void capture_buff_specs(const std::vector<BuffSpecEntry>& specs);

  // Faction specs (FactionSpecs, type 8) — factionId -> human-readable name
  void capture_faction_specs(const std::vector<std::pair<int64_t, std::string>>& specs); // {id, name}

  // Syndicate loyalty tier specs (LoyaltySpecs, type 122)
  struct LoyaltyBuffEntry { std::string faction; int32_t tier_index; int32_t max_tiers; };
  void capture_loyalty_specs(const std::vector<LoyaltyBuffEntry>& buff_map_entries,
                             const std::vector<int64_t>&           buff_ids);

  // Active buffs snapshot (GlobalActiveBuffs, type 69) — {buffId, level}
  void capture_active_buffs(const std::vector<std::pair<int64_t, int32_t>>& buffs);
} // namespace game_state_export
