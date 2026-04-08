#pragma once

#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace game_state_export
{
  void init();
  void export_now();  // Force immediate full export

  // Export functions
  void export_game_state();     // Full snapshot export (all per-section files)

  // Data capture functions called from sync hooks
  void capture_player_data(const nlohmann::json& data);
  void capture_player_alliance(const std::string& alliance_name, const std::string& alliance_tag,
                                int32_t level, int32_t member_count, int64_t power);
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

  // Station info from "starbase" Json blob key (arrives at login).
  // last_relocation_epoch = 0 means unknown. relocation_tokens = -1 means no change.
  void capture_station_info(int32_t home_system_id, int64_t last_relocation_epoch,
                            int64_t relocation_tokens);

  // Drydock assignments with ship status
  struct DrydockEntry {
    int32_t drydock_id = 0;   // 1-based sequential
    int64_t ship_id    = 0;
    int32_t state      = 0;   // DeployedFleetState: 0=idle,1=moving,2=warping,3=battling,4=recalling,5=stationary,6=entering_combat
    int32_t system_id  = 0;   // current system (node_address.system), 0 if unknown
    bool    is_damaged = false; // ship_dmg > 0
    bool    is_mining  = false; // is_mining field from my_deployed_fleets
    // Repair job info (if a repair is active for this ship)
    bool    repair_active = false;
    int64_t repair_finish_epoch = 0; // Unix epoch seconds when repair completes
    int32_t repair_seconds_remaining = 0; // derived at export time
    double  repair_progress = 0.0; // 0.0..100.0
    bool operator==(const DrydockEntry&) const = default;
  };
  void capture_drydock_assignments(const std::vector<DrydockEntry>& assignments);

  // Mid-session fleet status update: refresh state/system/is_mining/is_damaged
  // on existing cached drydock entries without requiring the full fleets key.
  // Map key is ship_id; value fields match DrydockEntry (state, system_id, etc.)
  struct FleetStatusUpdate {
    int32_t state      = 0;
    int32_t system_id  = 0;
    bool    is_damaged = false;
    bool    is_mining  = false;
  };
  void update_drydock_status(const std::unordered_map<int64_t, FleetStatusUpdate>& status_by_ship);

  // Fleet -> ship mapping used to resolve repair jobs to specific ships.
  // Called from process_json ("fleets" blob) so that repair jobs can be
  // resolved fleet_id -> ship_id when the Jobs payload arrives.
  void capture_fleet_ships(int64_t fleet_id, const std::vector<int64_t>& ship_ids);

  // Active job queues (research / build / scrap / repair).
  // Called on every JobResponse; replaces the entire queue snapshot.
  // The section is absent from the export when all four vectors are empty.
  struct QueueJobEntry {
    std::string job_type;     // "research" | "build" | "scrap" | "repair"
    int64_t     start_epoch    = 0;
    int32_t     duration_secs  = 0;
    int32_t     reduction_secs = 0;
    int64_t     ref_id         = 0;  // projectId / moduleId / hull_id / fleet_id
    int64_t     ref_id2        = 0;  // scrap: ship_id; others unused
    int32_t     level          = 0;
  };
  // Replaces the entire queue snapshot and also refreshes cached_ship_repairs
  // so that drydock entries show up-to-date repair progress.
  void capture_queues(std::vector<QueueJobEntry>&& jobs);

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

  // Faction favors (ConsumableSpecs, type 114)
  // Each favor is a RESEARCH_UNLOCK consumable: purchasing tier N unlocks researchId at level N.
  // faction_prefix: e.g. "Baj", "EXB", "S31"   favor_name: e.g. "Weapon_Damage"
  // max_tier: highest tier index seen for this researchId across all consumable entries
  struct FavorSpecEntry {
    int64_t     research_id;
    int32_t     tier;        // 1-based tier this consumable unlocks
    std::string faction_prefix;
    std::string favor_name;  // human-readable portion of the consumable name
  };
  void capture_favor_specs(const std::vector<FavorSpecEntry>& specs);

  // Research specs (ResearchSpecs type 66)
  // Builds a researchId -> researchTreeId lookup used to enrich faction_favors output.
  void capture_research_specs(std::unordered_map<int64_t, int64_t>&& research_faction_map);

  // Research tree -> faction name map (built from FactionSpec.researchTreeIds at login).
  void capture_tree_faction_map(std::unordered_map<int64_t, std::string>&& tree_faction_map);
  std::unordered_map<int64_t, std::string> get_research_tree_faction_map();

  // Ship tier specs (BaseShipTierSpecs type 49 + ShipTierSpecs type 50)
  // tier_level_caps:    tier    -> max ship level for that tier
  // hull_tier_durations: hull_id -> tier-up build time in seconds (per-hull override)
  // hull_cargo_stats:    hull_id -> tier -> { cargo_capacity, cargo_protection }
  struct CargoStats { float capacity = 0.f; float protection = 0.f; };
  void capture_ship_tier_specs(const std::unordered_map<int32_t, int32_t>& tier_level_caps,
                                const std::unordered_map<int64_t, int32_t>& hull_tier_durations,
                                const std::unordered_map<int64_t, std::unordered_map<int32_t, CargoStats>>& hull_cargo_stats);

  // Territory specs (TerritoryStaticData type 96)
  struct TerritoryWindow {
    int32_t weekday;       // 0=Sun … 6=Sat
    int32_t start_hour;    // UTC hour (0-23)
    int32_t duration_mins; // window length in minutes
  };
  struct TerritorySpec {
    int64_t                      territory_id = 0;
    int32_t                      tier         = 0;
    std::vector<TerritoryWindow> takeover_windows;
  };
  void capture_territory_specs(std::unordered_map<int64_t, TerritorySpec>&& specs);

  // Territory alliance slots (TerritoryAllianceSlots type 105)
  struct TerritorySlot {
    int64_t     territory_id = 0;
    std::string state;       // "owned" or "takeover"
  };
  void capture_territory_slots(std::vector<TerritorySlot>&& held, int32_t total_slots);

  // Blueprint specs (BlueprintSpecs type 21) — spec_id -> { name, parts_needed, hull_id }
  struct BlueprintSpecEntry {
    int64_t     spec_id     = 0;
    std::string name;
    int32_t     parts_needed = 0;  // required BP count to build
    int64_t     hull_id      = 0;  // outputReference — hull ID of the resulting ship
  };
  void capture_blueprint_specs(std::vector<BlueprintSpecEntry>&& specs);

  // Ship unlock blueprint counts from player inventory (INVENTORYITEMTYPE_INVENTORYBLUEPRINT)
  // spec_id = commonParams.refId, count = owned BP count
  void capture_ship_blueprints(int64_t spec_id, int64_t count);

} // namespace game_state_export
