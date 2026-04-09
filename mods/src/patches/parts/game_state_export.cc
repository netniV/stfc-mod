#include "game_state_export.h"
#include "id_mappings.h"
#include "../../config.h"
#include "../../file.h"
#include "../../version.h"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <condition_variable>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

using json = nlohmann::json;

namespace game_state_export
{

static std::thread export_thread;
static bool        should_stop = false;
static bool should_export_now = false;
static std::chrono::steady_clock::time_point last_export_request_tp;
static constexpr int EXPORT_DEBOUNCE_SECONDS = 10;
static std::mutex  export_request_mutex;
static std::condition_variable export_cv;

// Cached game data (current state)
static std::mutex game_data_mutex;
static json cached_player_data;
static std::unordered_map<int64_t, int64_t> cached_resources;
static std::unordered_map<int64_t, int32_t> cached_buildings;
static std::unordered_map<int64_t, json> cached_ships;
static std::unordered_map<int64_t, int32_t> cached_research;
static std::unordered_map<uint64_t, json> cached_officers;
// officer_id -> { trait_id -> level }
static std::unordered_map<uint64_t, std::unordered_map<uint64_t, int32_t>> cached_officer_traits;

// Peace shield: expiry as Unix epoch seconds (0 = not active), plus token inventory count
static int64_t cached_shield_expiry_epoch = 0;
static int64_t cached_shield_token_count  = 0;

// Station info from "starbase" blob key
static int32_t cached_home_system_id        = 0;
static int64_t cached_last_relocation_epoch = 0;  // 0 = unknown
static int64_t cached_relocation_tokens     = 0;
static int32_t cached_alliance_starbase_system_id = 0;

// Drydock assignments: drydock_id (1-based) -> player ship id
static std::vector<DrydockEntry> cached_drydock_assignments;

// Fleet -> ships map (fleetId -> ship ids) used to resolve repair jobs
static std::unordered_map<int64_t, std::vector<int64_t>> cached_fleet_to_ships;

// Repair jobs per-ship (ship_id -> job), refreshed on every JobResponse
struct RepairJobInfo {
  int64_t start_epoch = 0;
  int32_t duration_secs = 0;
  int32_t reduction_secs = 0;
};
static std::unordered_map<int64_t, RepairJobInfo> cached_ship_repairs;

// Active job queues snapshot (research / build / scrap / repair)
static std::vector<QueueJobEntry> cached_queues;

// Missions
static std::vector<std::pair<int64_t, int64_t>> cached_missions_active;    // {instance_id, mission_id}
static std::vector<int64_t>                     cached_missions_completed;

// Syndicate loyalty tier specs: buffId -> { tier, max_tiers }
struct LoyaltyBuffInfo { std::string faction; int32_t tier_index; int32_t max_tiers; };
static std::unordered_map<int64_t, LoyaltyBuffInfo> cached_loyalty_buff_map;

// Active buff snapshot: buffId -> level (from GlobalActiveBuffs)
static std::unordered_map<int64_t, int32_t> cached_active_buffs;

// Buff catalog: buffId -> BuffSpecEntry (from ShipBonusBuffSpecs type 51)
struct CachedBuffSpec {
  int32_t              modifier_code;
  int32_t              operation;
  int64_t              faction_id;
  bool                 show_percentage;
  std::vector<double>  ranked_values;
};
static std::unordered_map<int64_t, CachedBuffSpec> cached_buff_specs;

// Faction specs: factionId -> human-readable name (from FactionSpecs type 8)
static std::unordered_map<int64_t, std::string> cached_faction_specs;

// Faction favors spec: researchId -> { faction_prefix, favor_name, max_tier }
// Built from ConsumableSpecs (type 114) RESEARCH_UNLOCK entries.
// Multiple consumables share the same researchId (one per tier); max_tier is the highest seen.
struct FavorInfo { std::string faction_prefix; std::string favor_name; int32_t max_tier; };
static std::unordered_map<int64_t, FavorInfo> cached_favor_specs; // researchId -> info

// Research faction map: researchId -> faction_id
// Built from ResearchSpecs (type 66): ResearchProjectSpec.researchTreeId -> ResearchTreeSpec.factionId.
// Resolved to faction name at export time via cached_faction_specs.
static std::unordered_map<int64_t, int64_t> cached_research_faction_map; // researchId -> faction_id

// Research tree -> faction name: built from FactionSpec.researchTreeIds
static std::unordered_map<int64_t, std::string> cached_tree_faction_map; // researchTreeId -> faction name

// Ship tier specs (BaseShipTierSpecs type 49 + ShipTierSpecs type 50)
static std::unordered_map<int32_t, int32_t> cached_tier_level_caps;    // tier -> max ship level
static std::unordered_map<int64_t, int32_t> cached_hull_tier_durations; // hull_id -> duration secs
static std::unordered_map<int64_t, std::unordered_map<int32_t, CargoStats>> cached_hull_cargo_stats; // hull_id -> tier -> stats

// Territory (TerritoryStaticData type 96 + TerritoryAllianceSlots type 105)
static std::unordered_map<int64_t, TerritorySpec> cached_territory_specs; // territory_id -> spec
static std::vector<TerritorySlot>                 cached_territory_slots; // alliance held slots
static int32_t                                    cached_territory_total_slots = 0;
static std::unordered_map<int64_t, std::string>   cached_system_names;    // system_id -> name (from stfc_systems.json)

// Blueprint specs (BlueprintSpecs type 21) and ship unlock BP counts from inventory
static std::unordered_map<int64_t, BlueprintSpecEntry> cached_blueprint_specs; // spec_id -> spec
static std::unordered_map<int64_t, int64_t>            cached_ship_bp_counts;  // spec_id -> owned count

// Change tracking
static std::string last_full_export_time;
static std::chrono::steady_clock::time_point last_full_export_tp;
static std::chrono::steady_clock::time_point startup_time;
static constexpr int STARTUP_GRACE_PERIOD_SECONDS = 20;
static bool shield_scan_received = false; // suppresses alerts until first StarbaseDetailedScan

// Battlelog pending resolution â€” forward declarations so capture_player_data can call them
static std::unordered_set<std::string> pending_battlelog_resolution;
static std::mutex                      pending_battlelog_mtx;
static void resolve_pending_battlelog_outcomes(const std::string& our_name,
                                               const std::string& out_path);

// Gist sync state â€” declared here so capture_player_data can set the flag
static std::atomic<bool> gist_needs_player_name_sync{false};
static std::filesystem::path get_export_dir();
// One-time site upload function (forward declaration)
static void sync_site_index_to_gist(const std::filesystem::path& dir);
// Track whether site assets have been uploaded to the current gist already.
static bool site_assets_uploaded = false;
static std::string last_gist_id_synced;

static void request_immediate_export()
{
  // Don't trigger exports during startup grace period
  auto now = std::chrono::steady_clock::now();
  auto seconds_since_startup = std::chrono::duration_cast<std::chrono::seconds>(now - startup_time).count();

  if (seconds_since_startup < STARTUP_GRACE_PERIOD_SECONDS) {
    spdlog::debug("GameState: Skipping immediate export during startup grace period ({}/{}s)", 
                  seconds_since_startup, STARTUP_GRACE_PERIOD_SECONDS);
    return;
  }



  // Try to acquire the export mutex; if we cannot, set the flags without
  // blocking to avoid potential lock-order inversion with capture paths.
  if (export_request_mutex.try_lock()) {
    should_export_now      = true;
    last_export_request_tp = std::chrono::steady_clock::now();
    export_cv.notify_one();
    export_request_mutex.unlock();
  } else {
    should_export_now = true;
    last_export_request_tp = std::chrono::steady_clock::now();
    export_cv.notify_one();
  }
}

static void request_full_export()
{
  auto now = std::chrono::steady_clock::now();
  auto seconds_since_startup = std::chrono::duration_cast<std::chrono::seconds>(now - startup_time).count();
  if (seconds_since_startup < STARTUP_GRACE_PERIOD_SECONDS) return;

  if (export_request_mutex.try_lock()) {
    should_export_now      = true;
    last_export_request_tp = std::chrono::steady_clock::now();
    export_cv.notify_one();
    export_request_mutex.unlock();
  } else {
    should_export_now = true;
    last_export_request_tp = std::chrono::steady_clock::now();
    export_cv.notify_one();
  }
}

std::string get_iso8601_timestamp()
{
  auto        now       = std::chrono::system_clock::now();
  std::time_t now_c     = std::chrono::system_clock::to_time_t(now);
  auto        now_ms    = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
  std::tm     now_tm    = {};

#ifdef _WIN32
  localtime_s(&now_tm, &now_c);
#else
  localtime_r(&now_c, &now_tm);
#endif

  std::ostringstream oss;
  oss << std::put_time(&now_tm, "%Y-%m-%dT%H:%M:%S");
  oss << '.' << std::setfill('0') << std::setw(3) << now_ms.count();
  oss << std::put_time(&now_tm, "%z");

  return oss.str();
}

void capture_player_data(const nlohmann::json& data)
{
  std::string resolved_name;
  std::string battlelog_path;
  {
    std::scoped_lock lock(game_data_mutex);
    // Merge fields rather than overwrite - different sources populate different fields
    if (cached_player_data.is_null()) {
      cached_player_data = {{"name", ""}, {"alliance", ""}, {"ops_level", 0}, {"power", 0}, {"server", 0}};
    }
    bool changed = false;
    for (const auto& [key, value] : data.items()) {
      // Don't overwrite ops_level with 0 if we already have it from the OPERATIONS building
      if (key == "ops_level" && value == 0 && cached_player_data.value("ops_level", 0) > 0) continue;
      if (!cached_player_data.contains(key) || cached_player_data[key] != value) {
        cached_player_data[key] = value;
        changed = true;
      }
    }
    if (changed) {
      // If player name just became known, flag that the Gist needs a fresh sync
      // regardless of the rate-limit interval so it never serves an empty name.
      if (cached_player_data.value("name", "") != "") {
        gist_needs_player_name_sync.store(true);
      }
      request_full_export();
    }
    // Capture name and battlelog path for pending resolution outside the lock
    resolved_name  = cached_player_data.value("name", "");
    battlelog_path = (get_export_dir() / "battlelog.json").string();
  }

  // If name just became known and there are pending battlelog entries, resolve them
  if (!resolved_name.empty()) {
    bool has_pending;
    {
      std::scoped_lock lk(pending_battlelog_mtx);
      has_pending = !pending_battlelog_resolution.empty();
    }
    if (has_pending) {
      resolve_pending_battlelog_outcomes(resolved_name, battlelog_path);
    }
  }
}

void capture_player_alliance(const std::string& alliance_name, const std::string& alliance_tag,
                              int32_t level, int32_t member_count, int64_t power)
{
  std::scoped_lock lock(game_data_mutex);
  if (!cached_player_data.is_null()) {
    cached_player_data["alliance"] = {
      {"name",         alliance_name},
      {"tag",          alliance_tag},
      {"level",        level},
      {"member_count", member_count},
      {"power",        power}
    };
  }
}

void capture_resources(const nlohmann::json& data)
{
  std::scoped_lock lock(game_data_mutex);
  for (const auto& [str_id, resource] : data.items()) {
    auto id     = std::stoll(str_id);
    auto amount = resource["current_amount"].get<int64_t>();
    cached_resources[id] = amount;
  }
  // No immediate export requested: resource amounts change constantly due to
  // trickle income and would trigger an export every game tick. The interval
  // export (default 5 min) keeps resource data fresh enough.
}

void capture_buildings(const nlohmann::json& data)
{
  std::scoped_lock lock(game_data_mutex);
  spdlog::info("GameState capture_buildings: Received {} buildings, cached_buildings currently has {} entries", 
               data.size(), cached_buildings.size());

  bool changed = false;
  for (const auto& module : data) {
    const auto id = module["id"].get<int64_t>();
    const auto level = module["level"].get<int32_t>();

    auto it = cached_buildings.find(id);
    if (it == cached_buildings.end() || it->second != level) {
      cached_buildings[id] = level;
      changed = true;
    }

    // Building id=0 is OPERATIONS - its level is the player's ops level
    if (id == 0) {
      if (cached_player_data.is_null()) {
        cached_player_data = {{"name", ""}, {"alliance", ""}, {"power", 0}, {"server", 0}};
      }
      cached_player_data["ops_level"] = level;
      spdlog::info("GameState: Derived ops_level={} from OPERATIONS building", level);
    }
  }

  spdlog::info("GameState capture_buildings: After capture, cached_buildings has {} entries", cached_buildings.size());
  if (changed) {
    request_immediate_export();
  }
}

void capture_ships(const nlohmann::json& data)
{
  std::scoped_lock lock(game_data_mutex);
  spdlog::info("GameState capture_ships: Received {} ships, cached_ships currently has {} entries", 
               data.size(), cached_ships.size());

  bool changed = false;
  for (const auto& ship : data) {
    const auto id = ship["id"].get<int64_t>();
    json entry = {
      {"hull_id", ship["hull_id"]},
      {"tier", ship["tier"]},
      {"level", ship["level"]},
      {"level_percentage", ship["level_percentage"]},
      {"components", ship.value("components", json::array())}
    };
    auto it = cached_ships.find(id);
    if (it == cached_ships.end() || it->second != entry) {
      cached_ships[id] = std::move(entry);
      changed = true;
    }
  }

  spdlog::info("GameState capture_ships: After capture, cached_ships has {} entries", cached_ships.size());
  if (changed) {
    request_immediate_export();
  }
}

void capture_research(int64_t id, int32_t level)
{
  std::scoped_lock lock(game_data_mutex);
  cached_research[id] = level;

  // Only request export every 10th research item to avoid spam
  static int research_count = 0;
  if (++research_count % 10 == 0) {
    request_immediate_export();
  }
}

void capture_officers(uint64_t id, int32_t rank, int32_t level, int32_t shards)
{
  std::scoped_lock lock(game_data_mutex);
  cached_officers[id] = {
    {"rank", rank},
    {"level", level},
    {"shards", shards}
  };

  // Only request export every 10th officer to avoid spam
  static int officer_count = 0;
  if (++officer_count % 10 == 0) {
    request_immediate_export();
  }
}

void capture_officer_trait(uint64_t officer_id, uint64_t trait_id, int32_t level)
{
  std::scoped_lock lock(game_data_mutex);
  cached_officer_traits[officer_id][trait_id] = level;
}

void capture_peace_shield(int64_t active_expiry_epoch, int64_t token_count)
{
  bool expiry_changed = false;
  bool first_real_expiry = false;
  {
    std::scoped_lock lock(game_data_mutex);
    // Sentinels: -1 means "don't update this field"
    // - token_count == -1: called from StarbaseDetailedScan, expiry-only update
    // - active_expiry_epoch == -1: called from inventory path, token-count-only update
    if (token_count >= 0) {
      cached_shield_token_count = token_count;
    }
    if (active_expiry_epoch >= 0 && active_expiry_epoch != cached_shield_expiry_epoch) {
      first_real_expiry      = (cached_shield_expiry_epoch == 0 && active_expiry_epoch > 0);
      cached_shield_expiry_epoch = active_expiry_epoch;
      expiry_changed = true;
    } else if (active_expiry_epoch > 0 && active_expiry_epoch == cached_shield_expiry_epoch) {
      // Same active expiry as already cached â€” value hasn't changed but a new scan
      // confirmed the shield is still live; force an export so the gist stays current.
      expiry_changed = true;
    }
    spdlog::info("GameState: Captured peace shield expiry={}, tokens={}",
                 cached_shield_expiry_epoch, cached_shield_token_count);
  }
  // Mark that we've received at least one real scan so alerts are now valid.
  if (active_expiry_epoch >= 0) {
    std::scoped_lock lock(game_data_mutex);
    shield_scan_received = true;
  }

  // First time we learn a real expiry: force a full export so the JSON file
  // reflects the shield state immediately rather than waiting for next interval.
  // Subsequent changes go through the differential path.
  if (first_real_expiry) {
    request_full_export();
  } else if (expiry_changed) {
    request_immediate_export();
  }
}

void capture_station_info(int32_t home_system_id, int64_t last_relocation_epoch,
                          int64_t relocation_tokens)
{
  std::scoped_lock lock(game_data_mutex);
  bool changed = false;
  if (home_system_id != 0 && home_system_id != cached_home_system_id) {
    cached_home_system_id = home_system_id;
    changed = true;
  }
  if (last_relocation_epoch != 0 && last_relocation_epoch != cached_last_relocation_epoch) {
    cached_last_relocation_epoch = last_relocation_epoch;
    changed = true;
  }
  if (relocation_tokens >= 0 && relocation_tokens != cached_relocation_tokens) {
    cached_relocation_tokens = relocation_tokens;
    changed = true;
  }
  if (changed) {
    spdlog::info("GameState: station info: home_system={} last_relocation_epoch={} relocation_tokens={}",
                 cached_home_system_id, cached_last_relocation_epoch, cached_relocation_tokens);
    request_immediate_export();
  }
}

void capture_alliance_starbase_system(int32_t system_id)
{
  if (system_id == 0) return;
  std::scoped_lock lock(game_data_mutex);
  if (system_id == cached_alliance_starbase_system_id) return;
  cached_alliance_starbase_system_id = system_id;
  spdlog::info("GameState: alliance starbase system_id={}", system_id);
  request_immediate_export();
}

void capture_drydock_assignments(const std::vector<DrydockEntry>& assignments)
{
  std::scoped_lock lock(game_data_mutex);
  if (cached_drydock_assignments == assignments) return;
  cached_drydock_assignments = assignments;
  spdlog::info("GameState: Captured {} drydock assignments", assignments.size());
  request_immediate_export();
}

void update_drydock_status(const std::unordered_map<int64_t, FleetStatusUpdate>& status_by_ship)
{
  std::scoped_lock lock(game_data_mutex);
  bool changed = false;
  for (auto& d : cached_drydock_assignments) {
    auto it = status_by_ship.find(d.ship_id);
    if (it == status_by_ship.end()) continue;
    const auto& s = it->second;
    if (d.state != s.state || d.system_id != s.system_id ||
        d.is_damaged != s.is_damaged || d.is_mining != s.is_mining) {
      d.state      = s.state;
      d.system_id  = s.system_id;
      d.is_damaged = s.is_damaged;
      d.is_mining  = s.is_mining;
      changed = true;
    }
  }
  if (changed) {
    spdlog::info("GameState: drydock status updated mid-session ({} ships in update)",
                 status_by_ship.size());
    request_immediate_export();
  }
}

void capture_fleet_ships(int64_t fleet_id, const std::vector<int64_t>& ship_ids)
{
  std::scoped_lock lock(game_data_mutex);
  cached_fleet_to_ships[fleet_id] = ship_ids;
}

void capture_queues(std::vector<QueueJobEntry>&& jobs)
{
  std::scoped_lock lock(game_data_mutex);
  cached_queues = std::move(jobs);

  // Rebuild per-ship repair cache from the new snapshot so drydock entries
  // reflect the current repair state (clears stale entries on completion).
  cached_ship_repairs.clear();
  for (const auto& e : cached_queues) {
    if (e.job_type != "repair") continue;
    auto it = cached_fleet_to_ships.find(e.ref_id);
    if (it == cached_fleet_to_ships.end()) {
      spdlog::info("GameState: repair job for fleet {} has no fleet->ships mapping yet", e.ref_id);
      continue;
    }
    for (auto ship_id : it->second) {
      cached_ship_repairs[ship_id] = RepairJobInfo{e.start_epoch, e.duration_secs, e.reduction_secs};
    }
  }

  int research = 0, build = 0, scrap = 0, repair = 0;
  for (const auto& e : cached_queues) {
    if      (e.job_type == "research") ++research;
    else if (e.job_type == "build")    ++build;
    else if (e.job_type == "scrap")    ++scrap;
    else if (e.job_type == "repair")   ++repair;
  }
  spdlog::info("GameState: queues updated — research={} build={} scrap={} repair={}",
               research, build, scrap, repair);
  request_immediate_export();
}

void capture_missions_active(const std::vector<std::pair<int64_t, int64_t>>& missions)
{
  std::scoped_lock lock(game_data_mutex);
  if (cached_missions_active == missions) return;
  cached_missions_active = missions;
  spdlog::info("GameState: Captured {} active missions", missions.size());
  request_immediate_export();
}

void capture_missions_completed(const std::vector<int64_t>& mission_ids)
{
  std::scoped_lock lock(game_data_mutex);
  if (cached_missions_completed == mission_ids) return;
  cached_missions_completed = mission_ids;
  spdlog::info("GameState: Captured {} completed missions", mission_ids.size());
  request_immediate_export();
}

void capture_loyalty_specs(const std::vector<LoyaltyBuffEntry>& entries,
                            const std::vector<int64_t>&          buff_ids)
{
  std::scoped_lock lock(game_data_mutex);
  if (entries.size() != buff_ids.size()) {
    spdlog::warn("GameState: capture_loyalty_specs size mismatch ({} entries, {} ids)",
                 entries.size(), buff_ids.size());
    return;
  }
  // Accumulate across factions â€” each call covers one faction's full tier set.
  for (size_t i = 0; i < buff_ids.size(); ++i) {
    cached_loyalty_buff_map[buff_ids[i]] = {entries[i].faction, entries[i].tier_index, entries[i].max_tiers};
  }
  spdlog::info("GameState: capture_loyalty_specs: {} buff->faction mappings total",
               cached_loyalty_buff_map.size());
}

void capture_active_buffs(const std::vector<std::pair<int64_t, int32_t>>& buffs)
{
  std::scoped_lock lock(game_data_mutex);
  cached_active_buffs.clear();
  for (const auto& [id, level] : buffs) {
    cached_active_buffs[id] = level;
  }
  // Request export whenever buff catalog is available (covers both loyalty and faction favors)
  if (!cached_loyalty_buff_map.empty() || !cached_buff_specs.empty()) {
    request_immediate_export();
  }
}

void capture_buff_specs(const std::vector<BuffSpecEntry>& specs)
{
  std::scoped_lock lock(game_data_mutex);
  for (const auto& s : specs) {
    cached_buff_specs[s.buff_id] = {s.modifier_code, s.operation, s.faction_id,
                                    s.show_percentage, s.ranked_values};
  }
  spdlog::info("GameState: capture_buff_specs: {} total buff specs cached", cached_buff_specs.size());
}

void capture_faction_specs(const std::vector<std::pair<int64_t, std::string>>& specs)
{
  std::scoped_lock lock(game_data_mutex);
  for (const auto& [id, name] : specs) {
    cached_faction_specs[id] = name;
  }
  spdlog::info("GameState: capture_faction_specs: {} factions cached", cached_faction_specs.size());
}

void capture_research_specs(std::unordered_map<int64_t, int64_t>&& research_faction_map)
{
  std::scoped_lock lock(game_data_mutex);
  cached_research_faction_map = std::move(research_faction_map);
  spdlog::info("GameState: capture_research_specs: {} research->faction mappings cached",
               cached_research_faction_map.size());
}

void capture_tree_faction_map(std::unordered_map<int64_t, std::string>&& tree_faction_map)
{
  std::scoped_lock lock(game_data_mutex);
  cached_tree_faction_map = std::move(tree_faction_map);
  spdlog::info("GameState: capture_tree_faction_map: {} tree->faction mappings cached",
               cached_tree_faction_map.size());
}

std::unordered_map<int64_t, std::string> get_research_tree_faction_map()
{
  std::scoped_lock lock(game_data_mutex);
  return cached_tree_faction_map;
}

void capture_favor_specs(const std::vector<FavorSpecEntry>& specs)
{
  std::scoped_lock lock(game_data_mutex);
  for (const auto& s : specs) {
    auto& info = cached_favor_specs[s.research_id];
    info.faction_prefix = s.faction_prefix;
    info.favor_name     = s.favor_name;
    info.max_tier       = std::max(info.max_tier, s.tier);
  }
  // Count unique factions
  std::unordered_set<std::string> factions;
  for (const auto& [id, info] : cached_favor_specs) factions.insert(info.faction_prefix);
  spdlog::info("GameState: capture_favor_specs: {} favor types across {} factions",
               cached_favor_specs.size(), factions.size());
}

void capture_ship_tier_specs(const std::unordered_map<int32_t, int32_t>& tier_level_caps,
                              const std::unordered_map<int64_t, int32_t>& hull_tier_durations,
                              const std::unordered_map<int64_t, std::unordered_map<int32_t, CargoStats>>& hull_cargo_stats)
{
  std::scoped_lock lock(game_data_mutex);
  for (const auto& [tier, max_level] : tier_level_caps)
    cached_tier_level_caps[tier] = max_level;
  for (const auto& [hull_id, dur] : hull_tier_durations)
    cached_hull_tier_durations[hull_id] = dur;
  for (const auto& [hull_id, tiers] : hull_cargo_stats)
    for (const auto& [tier, stats] : tiers)
      cached_hull_cargo_stats[hull_id][tier] = stats;
  spdlog::info("GameState: capture_ship_tier_specs: {} tier caps, {} hull durations, {} hulls with cargo stats",
               cached_tier_level_caps.size(), cached_hull_tier_durations.size(), cached_hull_cargo_stats.size());
}

void capture_territory_specs(std::unordered_map<int64_t, TerritorySpec>&& specs)
{
  std::scoped_lock lock(game_data_mutex);
  cached_territory_specs = std::move(specs);
  spdlog::info("GameState: capture_territory_specs: {} territory specs cached",
               cached_territory_specs.size());
}

void capture_territory_slots(std::vector<TerritorySlot>&& held, int32_t total_slots)
{
  std::scoped_lock lock(game_data_mutex);
  cached_territory_slots       = std::move(held);
  cached_territory_total_slots = total_slots;
  spdlog::info("GameState: capture_territory_slots: {}/{} slots held",
               cached_territory_slots.size(), cached_territory_total_slots);
  request_immediate_export();
}

void capture_blueprint_specs(std::vector<BlueprintSpecEntry>&& specs)
{
  std::scoped_lock lock(game_data_mutex);
  cached_blueprint_specs.clear();
  for (auto& s : specs) {
    cached_blueprint_specs[s.spec_id] = std::move(s);
  }
  spdlog::info("GameState: capture_blueprint_specs: {} specs loaded", cached_blueprint_specs.size());
}

void capture_ship_blueprints(int64_t spec_id, int64_t count)
{
  std::scoped_lock lock(game_data_mutex);
  if (cached_ship_bp_counts[spec_id] == count) return;
  cached_ship_bp_counts[spec_id] = count;
  request_immediate_export();
}

// Human-readable name for a CLIENTMODIFIERTYPE_* code
static std::string modifier_code_name(int32_t code)
{
  switch (code) {
    case 0:   return "energy_damage";
    case 1:   return "kinetic_damage";
    case 2:   return "all_damage";
    case 3:   return "shots_per_attack";
    case 4:   return "all_load_speed";
    case 5:   return "all_reload_speed";
    case 6:   return "accuracy";
    case 7:   return "armor_piercing";
    case 8:   return "shield_piercing";
    case 9:   return "crit_chance";
    case 10:  return "crit_damage";
    case 11:  return "dodge";
    case 12:  return "armor";
    case 13:  return "shields";
    case 16:  return "shield_hp_repair";
    case 17:  return "hull_hp_repair";
    case 20:  return "parsteel_protection";
    case 21:  return "tritanium_protection";
    case 22:  return "dilithium_protection";
    case 23:  return "combat_parsteel_reward";
    case 24:  return "combat_tritanium_reward";
    case 25:  return "combat_dilithium_reward";
    case 26:  return "combat_intel_reward";
    case 40:  return "hull_hp";
    case 41:  return "shield_hp";
    case 60:  return "shield_hp_max";
    case 61:  return "hull_hp_max";
    case 62:  return "impulse_speed";
    case 63:  return "warp_speed";
    case 64:  return "warp_distance";
    case 66:  return "cargo_capacity";
    case 67:  return "cargo_protection";
    case 68:  return "impulse_speed_bonus";
    case 73:  return "all_defenses";
    case 74:  return "all_piercing";
    case 75:  return "shield_regen_time";
    case 76:  return "shield_mitigation";
    case 100: return "resource_protection";
    case 105: return "mining_rate";
    case 107: return "repair_cost";
    case 108: return "starbase_construction_speed";
    case 109: return "starbase_construction_cost";
    case 112: return "building_cost_efficiency";
    case 113: return "resource_storage";
    case 117: return "ship_construction_speed";
    case 118: return "ship_construction_cost";
    case 119: return "tier_up_speed";
    case 121: return "faction_store_loot_bonus";
    case 124: return "scan_ship";
    case 125: return "scan_station";
    case 126: return "scan_system";
    case 129: return "ship_scrap_resource_bonus";
    case 130: return "ship_scrap_loot_bonus";
    case 131: return "ship_scrap_speed";
    case 135: return "resource_producer_storage";
    case 136: return "warehouse_storage";
    case 137: return "vault_storage";
    case 139: return "ship_inventory_size";
    case 140: return "scan_cost";
    case 141: return "trade_tax";
    case 145: return "combat_pve_rewards";
    case 146: return "combat_outlaws_rewards";
    case 179: return "solo_armada_elite_credit_rewards";
    case 223: return "bypass_shields";
    case 225: return "hull_repair";
    case 233: return "simultaneous_armada";
    case 707: return "isolytic_damage";
    case 808: return "isolytic_defense";
    case 810: return "hazard_type_a_mitigation";
    case 811: return "hazard_type_b_mitigation";
    case 812: return "hazard_type_c_mitigation";
    case 815: return "hazard_type_e_mitigation";
    case 78010: return "wok_augment_all_loot_rewards";
    case 80003: return "m80_transogen_epic_loot";
    default:  return "modifier_" + std::to_string(code);
  }
}

json build_game_state_json()
{
  std::scoped_lock lock(game_data_mutex);
  const auto& o = Config::Get().game_state_options; // per-type opt-out flags for game state

  json j;

  j["meta"]["version"]         = "1.0.0";
  j["meta"]["exported_at"]     = get_iso8601_timestamp();
  j["meta"]["mod_version"]     = VER_FILE_VERSION_STR;
  j["meta"]["mappings_loaded"] = id_mappings::MappingCache::Get().is_loaded();

  // Player identity is always included - no per-type flag covers it
  if (!cached_player_data.is_null()) {
    j["player"] = cached_player_data;
  } else {
    j["player"] = {
      {"ops_level", 0},
      {"name", ""},
      {"alliance", ""},
      {"power", 0}
    };
  }

  // Alliance starbase system ID (from AllianceStarbaseConfig, arrives at login)
  if (cached_alliance_starbase_system_id != 0 &&
      j["player"].is_object() && j["player"]["alliance"].is_object()) {
    j["player"]["alliance"]["starbase_system_id"] = cached_alliance_starbase_system_id;
    auto sys_it = cached_system_names.find(static_cast<int64_t>(cached_alliance_starbase_system_id));
    if (sys_it != cached_system_names.end())
      j["player"]["alliance"]["starbase_system_name"] = sys_it->second;
  }

  // Syndicate level and XP come from resources - gated by resources flag
  if (o.resources) {
    static constexpr int64_t SYNDICATE_LEVEL_RESOURCE_ID = 1141922149LL;
    static constexpr int64_t SYNDICATE_XP_RESOURCE_ID    = 3374607211LL;
    auto level_it = cached_resources.find(SYNDICATE_LEVEL_RESOURCE_ID);
    auto xp_it    = cached_resources.find(SYNDICATE_XP_RESOURCE_ID);
    j["player"]["syndicate_level"] = (level_it != cached_resources.end()) ? level_it->second : 0;
    j["player"]["syndicate_xp"]    = (xp_it    != cached_resources.end()) ? xp_it->second    : 0;
  }

  // Buildings - gated by buildings flag
  j["buildings"] = json::array();
  if (o.buildings) {
    for (const auto& [id, level] : cached_buildings) {
      json building = {{"id", id}, {"level", level}};
      id_mappings::MappingCache::Get().enrich_building(building);
      j["buildings"].push_back(building);
    }
  }

  // Research - gated by research flag
  j["research"] = json::array();
  if (o.research) {
    for (const auto& [id, level] : cached_research) {
      json research = {{"id", id}, {"level", level}};
      id_mappings::MappingCache::Get().enrich_research(research);
      j["research"].push_back(research);
    }
  }

  // Ships - gated by ships flag
  j["ships"] = json::array();
  if (o.ships) {
    for (const auto& [id, ship_data] : cached_ships) {
      json ship_entry = ship_data;
      ship_entry["id"] = id;
      id_mappings::MappingCache::Get().enrich_ship(ship_entry);

      const int32_t tier    = ship_entry.value("tier", 0);
      const int64_t hull_id = ship_entry.value("hull_id", int64_t{0});

      auto cap_it = cached_tier_level_caps.find(tier);
      if (cap_it != cached_tier_level_caps.end())
        ship_entry["tier_max_level"] = cap_it->second;

      auto dur_it = cached_hull_tier_durations.find(hull_id);
      if (dur_it != cached_hull_tier_durations.end())
        ship_entry["tier_up_duration_secs"] = dur_it->second;

      auto cargo_hull_it = cached_hull_cargo_stats.find(hull_id);
      if (cargo_hull_it != cached_hull_cargo_stats.end()) {
        auto cargo_tier_it = cargo_hull_it->second.find(tier);
        if (cargo_tier_it != cargo_hull_it->second.end()) {
          ship_entry["cargo_capacity"]   = cargo_tier_it->second.capacity;
          ship_entry["cargo_protection"] = cargo_tier_it->second.protection;
        }
      }

      j["ships"].push_back(ship_entry);
    }
  }

  // Officers - gated by officer flag; trait ability levels also gated by traits flag
  j["officers"] = json::array();
  if (o.officer) {
    for (const auto& [id, officer_data] : cached_officers) {
      json officer_entry = officer_data;
      officer_entry["id"] = id;
      id_mappings::MappingCache::Get().enrich_officer(officer_entry);

      if (o.traits) {
        auto it = cached_officer_traits.find(id);
        if (it != cached_officer_traits.end() && officer_entry.contains("traits")) {
          for (auto& trait : officer_entry["traits"]) {
            uint64_t trait_id = trait["id"].get<uint64_t>();
            auto level_it = it->second.find(trait_id);
            trait["ability_level"] = (level_it != it->second.end()) ? level_it->second : 0;
          }
        }
      }

      j["officers"].push_back(officer_entry);
    }
  }

  // Resources - gated by resources flag
  j["resources"] = json::array();
  if (o.resources) {
    for (const auto& [id, amount] : cached_resources) {
      json resource = {{"id", id}, {"amount", amount}};
      id_mappings::MappingCache::Get().enrich_resource(resource, id);
      j["resources"].push_back(resource);
    }
  }

  // Faction reputation - from resources, gated by resources flag
  j["faction_reputation"] = json::array();
  if (o.resources) {
    for (const auto& [id, amount] : cached_resources) {
      auto mapping = id_mappings::MappingCache::Get().get_resource(id);
      if (!mapping) continue;
      const std::string& name   = mapping->name;
      const std::string  prefix = "Resource_FactionPoint_";
      if (name.rfind(prefix, 0) == 0) {
        j["faction_reputation"].push_back({
          {"faction",     name.substr(prefix.size())},
          {"resource_id", id},
          {"points",      amount}
        });
      }
    }
  }

  // Faction store tokens - from resources, gated by resources flag
  j["faction_store_tokens"] = json::array();
  if (o.resources) {
    for (const auto& [id, amount] : cached_resources) {
      if (amount == 0) continue;
      auto mapping = id_mappings::MappingCache::Get().get_resource(id);
      if (!mapping) continue;
      const std::string& name      = mapping->name;
      const std::string  ft_prefix = "Resource_FactionToken_";
      if (name.rfind(ft_prefix, 0) == 0) {
        j["faction_store_tokens"].push_back({
          {"token",       name.substr(ft_prefix.size())},
          {"resource_id", id},
          {"amount",      amount}
        });
      }
    }
  }

  // Armada credits - from resources, gated by resources flag
  j["armada_credits"] = json::array();
  if (o.resources) {
    for (const auto& [id, amount] : cached_resources) {
      if (amount == 0) continue;
      auto mapping = id_mappings::MappingCache::Get().get_resource(id);
      if (!mapping) continue;
      const std::string& name       = mapping->name;
      const std::string  sa_prefix  = "Resource_Faction_SArmada_Credit_";
      const std::string  dir_prefix = "Resource_Faction_SArmadaDir_";
      if (name.rfind(sa_prefix, 0) == 0 || name.rfind(dir_prefix, 0) == 0) {
        j["armada_credits"].push_back({
          {"resource_id", id},
          {"name",        name},
          {"amount",      amount}
        });
      }
    }
  }

  // Syndicate loyalty buffs - gated by buffs flag
  j["syndicate_loyalty_buffs"] = json::array();
  if (o.buffs) {
    std::map<std::string, json> by_faction;
    for (const auto& [buff_id, level] : cached_active_buffs) {
      auto it = cached_loyalty_buff_map.find(buff_id);
      if (it == cached_loyalty_buff_map.end()) continue;
      const auto& info = it->second;
      by_faction[info.faction].push_back({
        {"tier",      info.tier_index},
        {"max_tiers", info.max_tiers},
        {"buff_id",   buff_id},
        {"level",     level}
      });
    }
    for (auto& [faction, tiers] : by_faction) {
      std::sort(tiers.begin(), tiers.end(), [](const json& a, const json& b) {
        return a["tier"].get<int32_t>() < b["tier"].get<int32_t>();
      });
      j["syndicate_loyalty_buffs"].push_back({{"faction", faction}, {"tiers", tiers}});
    }
  }

  // Faction favors - uses research levels (research flag) and consumable specs (buffs flag)
  j["faction_favors"] = json::array();
  if (o.research && o.buffs) {
    std::map<std::string, json> by_faction;
    for (const auto& [research_id, info] : cached_favor_specs) {
      std::string faction_name = info.faction_prefix;
      auto tree_id_it = cached_research_faction_map.find(research_id);
      if (tree_id_it != cached_research_faction_map.end()) {
        auto name_it = cached_tree_faction_map.find(tree_id_it->second);
        if (name_it != cached_tree_faction_map.end() && !name_it->second.empty())
          faction_name = name_it->second;
      }
      auto res_it = cached_research.find(research_id);
      const int32_t player_tier = (res_it != cached_research.end()) ? res_it->second : 0;
      by_faction[faction_name].push_back({
        {"favor",       info.favor_name},
        {"tier",        player_tier},
        {"max_tier",    info.max_tier},
        {"research_id", research_id}
      });
    }
    for (auto& [faction, favors] : by_faction) {
      std::sort(favors.begin(), favors.end(), [](const json& a, const json& b) {
        return a["favor"].get<std::string>() < b["favor"].get<std::string>();
      });
      j["faction_favors"].push_back({{"faction", faction}, {"favors", favors}});
    }
  }

  // Full buff catalog - gated by buffs flag
  j["buff_catalog"] = json::array();
  if (o.buffs) {
    for (const auto& [buff_id, spec] : cached_buff_specs) {
      std::string faction_name;
      if (spec.faction_id > 0) {
        auto fac_it = cached_faction_specs.find(spec.faction_id);
        faction_name = (fac_it != cached_faction_specs.end()) ? fac_it->second
                                                               : "faction_" + std::to_string(spec.faction_id);
      }
      json entry = {
        {"buff_id",         buff_id},
        {"modifier",        modifier_code_name(spec.modifier_code)},
        {"modifier_code",   spec.modifier_code},
        {"operation",       spec.operation},
        {"show_percentage", spec.show_percentage},
        {"ranked_values",   spec.ranked_values}
      };
      if (!faction_name.empty()) entry["faction"] = faction_name;
      j["buff_catalog"].push_back(std::move(entry));
    }
  }

  // Blueprint parts - from resources, gated by resources flag
  j["blueprints"] = json::array();
  if (o.resources) {
    for (const auto& [id, amount] : cached_resources) {
      auto mapping = id_mappings::MappingCache::Get().get_resource(id);
      if (!mapping) continue;
      const std::string& name = mapping->name;
      if (name.find("_Parts_") != std::string::npos || name.find("_Parts") == name.size() - 6) {
        j["blueprints"].push_back({
          {"resource_id", id},
          {"name",        name},
          {"amount",      amount}
        });
      }
    }
  }

  // Ship unlock blueprint counts - gated by inventory flag
  j["ship_blueprints"] = json::array();
  if (o.inventory) {
    for (const auto& [spec_id, count] : cached_ship_bp_counts) {
      auto it = cached_blueprint_specs.find(spec_id);
      json entry;
      entry["spec_id"] = spec_id;
      entry["amount"]  = count;
      if (it != cached_blueprint_specs.end()) {
        entry["name"]         = it->second.name;
        entry["parts_needed"] = it->second.parts_needed;
        if (it->second.hull_id != 0) entry["hull_id"] = it->second.hull_id;
      }
      j["ship_blueprints"].push_back(std::move(entry));
    }
  }

  // Station peace shield (always included)
  {
    auto now_epoch = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    bool shield_active = cached_shield_expiry_epoch > now_epoch;

    static const std::unordered_set<int64_t> shield_token_resource_ids = {
      3788095604LL, 3679054867LL, 3058388990LL, 3257212914LL, 1105830521LL,
      178169676LL,  1905604904LL, 985389969LL,  166906622LL,  676333576LL,
    };
    int64_t token_count = 0;
    if (o.resources) {
      for (const auto& [id, amount] : cached_resources) {
        if (shield_token_resource_ids.count(id)) token_count += amount;
      }
    }

    json shield_json;
    shield_json["active"]      = shield_active;
    shield_json["token_count"] = token_count;
    if (shield_active) {
      std::time_t expiry_t = static_cast<std::time_t>(cached_shield_expiry_epoch);
      std::tm expiry_tm    = {};
#ifdef _WIN32
      gmtime_s(&expiry_tm, &expiry_t);
#else
      gmtime_r(&expiry_t, &expiry_tm);
#endif
      std::ostringstream oss;
      oss << std::put_time(&expiry_tm, "%Y-%m-%dT%H:%M:%SZ");
      shield_json["expires_at"]        = oss.str();
      shield_json["expires_epoch"]     = cached_shield_expiry_epoch;
      shield_json["seconds_remaining"] = cached_shield_expiry_epoch - now_epoch;
    } else {
      shield_json["expires_at"]        = nullptr;
      shield_json["expires_epoch"]     = nullptr;
      shield_json["seconds_remaining"] = 0;
    }
    j["station"]["peace_shield"] = shield_json;
  }

  // Station info from "starbase" blob key
  if (cached_home_system_id != 0) {
    j["station"]["home_system_id"] = cached_home_system_id;
    auto sys_it = cached_system_names.find(static_cast<int64_t>(cached_home_system_id));
    if (sys_it != cached_system_names.end())
      j["station"]["home_system_name"] = sys_it->second;
  }

  if (cached_last_relocation_epoch != 0) {
    std::time_t reloc_t  = static_cast<std::time_t>(cached_last_relocation_epoch);
    std::tm     reloc_tm = {};
#ifdef _WIN32
    gmtime_s(&reloc_tm, &reloc_t);
#else
    gmtime_r(&reloc_t, &reloc_tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&reloc_tm, "%Y-%m-%dT%H:%M:%SZ");
    j["station"]["last_relocation_at"] = oss.str();

    auto now_epoch = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    double days = static_cast<double>(now_epoch - cached_last_relocation_epoch) / 86400.0;
    j["station"]["days_in_system"] = std::round(days * 10.0) / 10.0;  // 1 decimal place
  }

  j["station"]["relocation_tokens"] = cached_relocation_tokens;
  if (o.resources) {
    static constexpr int64_t RELOCATION_TOKEN_RESOURCE_ID = 1638723607LL;
    auto it = cached_resources.find(RELOCATION_TOKEN_RESOURCE_ID);
    if (it != cached_resources.end())
      j["station"]["relocation_tokens"] = it->second;
  }

  // Active missions (always present even when missions flag is false - missions file is separate)
  j["missions_active"] = json::array();
  for (const auto& [instance_id, mission_id] : cached_missions_active) {
    j["missions_active"].push_back({{"instance_id", instance_id}, {"mission_id", mission_id}});
  }

  j["missions_completed"] = json::array();
  for (const auto mission_id : cached_missions_completed) {
    j["missions_completed"].push_back(mission_id);
  }

  // Drydock letter: 1=A, 2=B ... 26=Z, 27=AA (spreadsheet column style)
  auto drydock_letter = [](int32_t id) -> std::string {
    std::string result;
    while (id > 0) {
      id--;
      result = static_cast<char>('A' + (id % 26)) + result;
      id /= 26;
    }
    return result;
  };

  static const std::unordered_map<int32_t, std::string> FLEET_STATE_NAMES = {
    {0, "idle"}, {1, "moving"}, {2, "warping"}, {3, "battling"},
    {4, "recalling"}, {5, "stationary"}, {6, "entering_combat"}
  };

  j["drydocks"] = json::array();
  for (const auto& d : cached_drydock_assignments) {
    json entry;
    entry["drydock_id"] = d.drydock_id;
    entry["letter"]     = drydock_letter(d.drydock_id);
    entry["ship_id"]    = d.ship_id;
    auto state_it = FLEET_STATE_NAMES.find(d.state);
    std::string status = (state_it != FLEET_STATE_NAMES.end()) ? state_it->second : "unknown";
    if (d.is_mining) status = "mining";
    entry["status"]     = status;
    entry["is_damaged"] = d.is_damaged;
    entry["is_mining"]  = d.is_mining;
    if (d.system_id != 0) {
      entry["system_id"] = d.system_id;
      auto sys_it = cached_system_names.find(static_cast<int64_t>(d.system_id));
      if (sys_it != cached_system_names.end())
        entry["system_name"] = sys_it->second;
    }
    // A ship is docked at home station when it is in the home system and idle (state=0, not mining)
    entry["is_at_home_station"] = (cached_home_system_id != 0 &&
                                   d.system_id == cached_home_system_id &&
                                   d.state == 0 && !d.is_mining);

    // Repair info from cached_ship_repairs, if present
    // (game_data_mutex already held by build_game_state_json outer lock)
    {
      auto it = cached_ship_repairs.find(d.ship_id);
      if (it != cached_ship_repairs.end()) {
        const auto& r = it->second;
        entry["repair_active"] = true;
        int64_t finish = r.start_epoch + r.duration_secs - r.reduction_secs;
        entry["repair_finish_epoch"] = finish;
        int64_t now_epoch = static_cast<int64_t>(
          std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        int64_t secs_left = finish > now_epoch ? (finish - now_epoch) : 0;
        entry["repair_seconds_remaining"] = static_cast<int32_t>(secs_left);
        double progress = 0.0;
        if (r.duration_secs > 0) {
          double done = static_cast<double>(r.duration_secs - (r.reduction_secs + secs_left));
          progress = std::clamp(done / static_cast<double>(r.duration_secs) * 100.0, 0.0, 100.0);
        }
        entry["repair_progress"] = std::round(progress * 10.0) / 10.0; // 1 dp
      } else {
        entry["repair_active"] = false;
        entry["repair_finish_epoch"] = nullptr;
        entry["repair_seconds_remaining"] = 0;
        entry["repair_progress"] = 0.0;
      }
    }
    if (o.ships) {
      auto ship_it = cached_ships.find(d.ship_id);
      if (ship_it != cached_ships.end() && ship_it->second.contains("hull_id")) {
        int64_t hull_id = ship_it->second["hull_id"].get<int64_t>();
        auto mapping = id_mappings::MappingCache::Get().get_ship(hull_id);
        if (mapping) entry["ship_name"] = mapping->name;
      }
    }
    j["drydocks"].push_back(entry);
  }

  // Active job queues (research / build / scrap).
  // Repair jobs are not included here — repair status is reflected per-drydock entry.
  // Section is omitted entirely when all queues are idle.
  const bool has_non_repair_queues = std::any_of(cached_queues.begin(), cached_queues.end(),
    [](const QueueJobEntry& e) { return e.job_type != "repair"; });
  if (has_non_repair_queues) {
    int64_t q_now = static_cast<int64_t>(
      std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    auto epoch_to_iso = [](int64_t epoch) -> std::string {
      std::time_t t  = static_cast<std::time_t>(epoch);
      std::tm     tm = {};
#ifdef _WIN32
      gmtime_s(&tm, &t);
#else
      gmtime_r(&t, &tm);
#endif
      std::ostringstream oss;
      oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
      return oss.str();
    };

    json queues = json::object();
    for (const char* key : {"research", "build", "scrap"})
      queues[key] = json::array();

    for (const auto& e : cached_queues) {
      if (e.job_type == "repair") continue;
      int64_t finish    = (e.start_epoch > 0)
        ? e.start_epoch + e.duration_secs - e.reduction_secs : 0;
      int64_t secs_left = (finish > q_now) ? (finish - q_now) : 0;

      json qe;
      qe["start_time"]        = (e.start_epoch > 0) ? json(epoch_to_iso(e.start_epoch)) : json(nullptr);
      qe["duration_secs"]     = e.duration_secs;
      qe["reduction_secs"]    = e.reduction_secs;
      qe["finish_time"]       = (finish > 0) ? json(epoch_to_iso(finish)) : json(nullptr);
      qe["seconds_remaining"] = static_cast<int32_t>(secs_left);

      if (e.job_type == "research") {
        qe["project_id"] = e.ref_id;
        qe["level"]      = e.level;
        auto m = id_mappings::MappingCache::Get().get_research(e.ref_id);
        if (m) qe["project_name"] = m->name;
      } else if (e.job_type == "build") {
        qe["module_id"] = e.ref_id;
        qe["level"]     = e.level;
        auto m = id_mappings::MappingCache::Get().get_building(e.ref_id);
        if (m) qe["module_name"] = m->name;
      } else if (e.job_type == "scrap") {
        qe["hull_id"]  = e.ref_id;
        qe["ship_id"]  = e.ref_id2;
        qe["level"]    = e.level;
        auto m = id_mappings::MappingCache::Get().get_ship(e.ref_id);
        if (m) qe["ship_name"] = m->name;
      }

      queues[e.job_type].push_back(std::move(qe));
    }
    j["queues"] = std::move(queues);
  }

  // Annotate ships array with drydock letter and status
  if (o.ships) {
    for (auto& ship_entry : j["ships"]) {
      int64_t sid = ship_entry["id"].get<int64_t>();
      for (const auto& d : cached_drydock_assignments) {
        if (d.ship_id == sid) {
            auto state_it = FLEET_STATE_NAMES.find(d.state);
            std::string status = (state_it != FLEET_STATE_NAMES.end()) ? state_it->second : "unknown";
            if (d.is_mining) status = "mining";
            ship_entry["drydock"]    = drydock_letter(d.drydock_id);
            ship_entry["drydock_id"] = d.drydock_id;
            ship_entry["status"]     = status;
            ship_entry["is_mining"]  = d.is_mining;

            // Repair info for this ship
            // (game_data_mutex already held by build_game_state_json outer lock)
            {
              auto it = cached_ship_repairs.find(d.ship_id);
              if (it != cached_ship_repairs.end()) {
                const auto& r = it->second;
                ship_entry["repair_active"] = true;
                int64_t finish = r.start_epoch + r.duration_secs - r.reduction_secs;
                ship_entry["repair_finish_epoch"] = finish;
                int64_t now_epoch = static_cast<int64_t>(
                  std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
                int64_t secs_left = finish > now_epoch ? (finish - now_epoch) : 0;
                ship_entry["repair_seconds_remaining"] = static_cast<int32_t>(secs_left);
                double progress = 0.0;
                if (r.duration_secs > 0) {
                  double done = static_cast<double>(r.duration_secs - (r.reduction_secs + secs_left));
                  progress = std::clamp(done / static_cast<double>(r.duration_secs) * 100.0, 0.0, 100.0);
                }
                ship_entry["repair_progress"] = std::round(progress * 10.0) / 10.0; // 1 dp
              } else {
                ship_entry["repair_active"] = false;
                ship_entry["repair_finish_epoch"] = nullptr;
                ship_entry["repair_seconds_remaining"] = 0;
                ship_entry["repair_progress"] = 0.0;
              }
            }

            break;
        }
      }
    }
  }

  spdlog::info("GameState JSON structure built: {} buildings, {} research, {} ships, {} officers, {} resources, {} drydocks",
                cached_buildings.size(), cached_research.size(), cached_ships.size(),
                cached_officers.size(), cached_resources.size(), cached_drydock_assignments.size());

  // Alliance territory (always included)
  {
    static const std::unordered_map<int32_t, std::string> WEEKDAY_NAMES = {
      {0,"Sun"},{1,"Mon"},{2,"Tue"},{3,"Wed"},{4,"Thu"},{5,"Fri"},{6,"Sat"}
    };
    j["territory"]["total_slots"] = cached_territory_total_slots;
    j["territory"]["used_slots"]  = static_cast<int>(cached_territory_slots.size());
    j["territory"]["held"]        = json::array();
    for (const auto& slot : cached_territory_slots) {
      json entry;
      entry["territory_id"] = slot.territory_id;
      entry["state"]        = slot.state;
      auto spec_it = cached_territory_specs.find(slot.territory_id);
      if (spec_it != cached_territory_specs.end()) {
        const auto& spec = spec_it->second;
        // Derive territory name from the first node whose system name ends with Alpha/Beta/Gamma.
        // System names from stfc_systems.json look like "Framtid Alpha (20)" — strip suffix.
        static const std::vector<std::string> SUFFIXES = {" Alpha", " Beta", " Gamma"};
        for (int64_t nid : spec.node_ids) {
          auto sys_it = cached_system_names.find(nid);
          if (sys_it == cached_system_names.end()) continue;
          const std::string& sys_name = sys_it->second;
          for (const auto& sfx : SUFFIXES) {
            auto pos = sys_name.find(sfx);
            if (pos != std::string::npos) {
              entry["name"] = sys_name.substr(0, pos);
              break;
            }
          }
          if (entry.contains("name")) break;
        }
        entry["tier"] = spec.tier;
        entry["node_ids"] = spec.node_ids;
        entry["takeover_windows"] = json::array();
        for (const auto& w : spec.takeover_windows) {
          auto day_it = WEEKDAY_NAMES.find(w.weekday);
          json win;
          win["weekday"]        = w.weekday;
          win["weekday_name"]   = (day_it != WEEKDAY_NAMES.end()) ? day_it->second : "";
          win["start_hour_utc"] = w.start_hour;
          win["duration_mins"]  = w.duration_mins;
          entry["takeover_windows"].push_back(std::move(win));
        }
      }
      j["territory"]["held"].push_back(std::move(entry));
    }
  }

  return j;
}

static void sync_to_gist(const std::string& file_path, const std::string& gist_filename)
{
  const auto& gist = Config::Get().game_state_github;
  if (!gist.enabled || gist.gist_id.empty() || gist.token.empty()) {
    return;
  }

  std::ifstream in(file_path);
  if (!in.is_open()) {
    spdlog::warn("Gist sync: Could not read {}", file_path);
    return;
  }
  std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  in.close();

  const std::string url = "https://api.github.com/gists/" + gist.gist_id;
  const nlohmann::json body = {
    {"files", {{gist_filename, {{"content", content}}}}}
  };

  auto response = cpr::Patch(
    cpr::Url{url},
    cpr::Header{
      {"Authorization", "token " + gist.token},
      {"Accept",        "application/vnd.github.v3+json"},
      {"Content-Type",  "application/json"}
    },
    cpr::Body{body.dump()}
  );

  if (response.status_code == 200) {
    spdlog::info("Gist sync: Updated {} in gist {}", gist_filename, gist.gist_id);
  } else {
    spdlog::warn("Gist sync: Failed to update {} - HTTP {} {}",
                 gist_filename, response.status_code, response.status_line);
  }
}

// Sync all game state export files to Gist in a single PATCH request.
// Using one request avoids repeated 403s when new filenames don't yet
// exist in the gist (GitHub creates missing files on PATCH automatically
// when all filenames are included together).
// A minimum interval between syncs is enforced to avoid rate limiting.
static std::chrono::steady_clock::time_point last_gist_sync_tp;
static bool last_gist_sync_had_empty_name  = false;
static constexpr int GIST_SYNC_MIN_INTERVAL_SECONDS = 30;

static void sync_all_to_gist(const std::filesystem::path& dir)
{
  const auto& gist = Config::Get().game_state_github;
  if (!gist.enabled || gist.gist_id.empty() || gist.token.empty()) return;

  // Enforce minimum interval between Gist syncs to avoid rate limiting.
  // Exception: if the previous sync went out with an empty player name and
  // we now have a real name, sync immediately so the Gist reflects it.
  auto now = std::chrono::steady_clock::now();
  bool player_name_just_arrived = false;
  if (last_gist_sync_had_empty_name) {
    std::scoped_lock lk(game_data_mutex);
    player_name_just_arrived = !cached_player_data.value("name", "").empty();
  }
  bool force_sync = player_name_just_arrived || gist_needs_player_name_sync.load();
  if (!force_sync && last_gist_sync_tp != std::chrono::steady_clock::time_point{}) {
    auto secs_since_last = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_gist_sync_tp).count();
    if (secs_since_last < GIST_SYNC_MIN_INTERVAL_SECONDS) {
      spdlog::debug("Gist sync: Skipping ({}s since last sync, min interval {}s)",
                    secs_since_last, GIST_SYNC_MIN_INTERVAL_SECONDS);
      return;
    }
  }

  // Map of gist filename -> local file path
  const std::vector<std::pair<std::string, std::filesystem::path>> files = {
    {gist.filename_player,    dir / "player.json"},
    {gist.filename_buildings, dir / "buildings.json"},
    {gist.filename_ships,     dir / "ships.json"},
    {gist.filename_resources, dir / "resources.json"},
    {gist.filename_research,  dir / "research.json"},
    {gist.filename_officers,  dir / "officers.json"},
    {gist.filename_missions,  dir / "missions.json"},
    {gist.filename_faction,   dir / "faction.json"},
    {gist.filename_buffs,     dir / "buffs.json"},
    {gist.filename_territory, dir / "territory.json"},
    {gist.filename_manifest,  dir / "manifest.json"},
  };

  // Build file list object. Include regular export files plus any files
  // found under the exported `site/` directory (copied in earlier).
  // To avoid wasting API calls and bandwidth, include site assets only
  // on the first successful sync for the configured gist ID.
  json files_obj = json::object();
  for (const auto& [gist_name, local_path] : files) {
    std::ifstream in(local_path);
    if (!in.is_open()) {
      spdlog::warn("Gist sync: Could not read {}", local_path.string());
      continue;
    }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    files_obj[gist_name] = {{"content", content}};
  }

  // Include any site assets present under the export dir `site/`.
  // Only include them if we haven't already uploaded site assets for this gist id.
  bool included_site_assets = false;
  std::filesystem::path site_dir = dir / "site";
  if (!gist.gist_id.empty() && last_gist_id_synced != gist.gist_id) {
    // New gist id configured since last upload -> reset site flag so initial upload will include site
    site_assets_uploaded = false;
  }
  if (!site_assets_uploaded && std::filesystem::exists(site_dir)) {
    for (auto& entry : std::filesystem::recursive_directory_iterator(site_dir)) {
      if (!entry.is_regular_file()) continue;
      try {
        auto rel = std::filesystem::relative(entry.path(), dir).generic_string();
        std::string gist_name = rel;
        // GitHub Gist filenames do not accept path separators; convert to a flat name.
        for (auto& c : gist_name) if (c == '/' || c == '\\') c = '_';
        std::ifstream in(entry.path());
        if (!in.is_open()) { spdlog::warn("Gist sync: Could not read site asset {}", entry.path().string()); continue; }
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        files_obj[gist_name] = {{"content", content}};
        included_site_assets = true;
      } catch (const std::exception& e) {
        spdlog::warn("Gist sync: Exception while adding site asset {}: {}", entry.path().string(), e.what());
      }
    }
  }

  if (files_obj.empty()) return;

  const std::string url = "https://api.github.com/gists/" + gist.gist_id;
  const json body = {{"files", files_obj}};

  // Log filenames and sizes we're about to upload for diagnostic purposes
  try {
    for (auto it = files_obj.begin(); it != files_obj.end(); ++it) {
      try {
        const std::string fname = it.key();
        const std::string content = it.value().value("content", std::string());
        spdlog::info("Gist sync: Preparing upload file '{}' ({} bytes)", fname, content.size());
      } catch (...) {}
    }
  } catch (...) {}

  auto response = cpr::Patch(
    cpr::Url{url},
    cpr::Header{
      {"Authorization", "token " + gist.token},
      {"Accept",        "application/vnd.github.v3+json"},
      {"Content-Type",  "application/json"}
    },
    cpr::Body{body.dump()}
  );

  if (response.status_code == 200) {
    last_gist_sync_tp = std::chrono::steady_clock::now();
    gist_needs_player_name_sync.store(false);
    {
      std::scoped_lock lk(game_data_mutex);
      last_gist_sync_had_empty_name = cached_player_data.value("name", "").empty();
    }
    // If we uploaded site assets in this request, mark them as uploaded for this gist id so
    // subsequent syncs skip uploading the static site files (saves bandwidth and API calls).
    try {
      if (files_obj.contains("site/index.html") || files_obj.size() > 0) {
        // Determine if any of the uploaded filenames start with "site/".
        for (auto it = files_obj.begin(); it != files_obj.end(); ++it) {
          const std::string fname = it.key();
          if (fname.rfind("site/", 0) == 0) {
            site_assets_uploaded = true;
            last_gist_id_synced = gist.gist_id;
            spdlog::info("Gist sync: Marked site assets uploaded for gist {}", gist.gist_id);
            break;
          }
        }
      }
    } catch (...) {}
    // Auto-populate username from the API response if not already set
    if (Config::Get().game_state_github.username.empty()) {
      try {
        auto resp_json = nlohmann::json::parse(response.text);
        if (resp_json.contains("owner") && resp_json["owner"].contains("login")) {
          Config::Get().game_state_github.username = resp_json["owner"]["login"].get<std::string>();
          spdlog::info("Gist sync: Resolved GitHub username='{}' â€” re-writing manifest with correct URLs",
                       Config::Get().game_state_github.username);
          // Re-write manifest.json on disk so URLs are correct without waiting for the next full export
          request_immediate_export();
        }
      } catch (...) {}
    }
    spdlog::info("Gist sync: Updated {} game state files in gist {}", files_obj.size(), gist.gist_id);
  } else if (response.status_code == 403) {
    // 403 is typically a rate limit â€” parse message if available
    std::string reason = "Forbidden";
    try {
      auto body_json = nlohmann::json::parse(response.text);
      if (body_json.contains("message")) reason = body_json["message"].get<std::string>();
    } catch (...) {}
    spdlog::warn("Gist sync: Batch update failed (403): {}", reason);
  } else {
    spdlog::warn("Gist sync: Batch update failed - HTTP {} {}: {}",
                 response.status_code, response.status_line,
                 response.text.substr(0, 200));
  }
}

// Returns the export directory (always community_patch/game_state_exports/), creating it if needed.
static std::filesystem::path get_export_dir()
{
  auto& cfg = Config::Get();
  std::filesystem::path base = cfg.game_state_path.empty()
    ? std::filesystem::path(".")
    : std::filesystem::path(cfg.game_state_path);
  auto dir = base / "community_patch" / "game_state_exports";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    spdlog::error("GameState export: Failed to create directory {}: {}", dir.string(), ec.message());
    // Fall back to flat subfolder
    dir = std::filesystem::path("community_patch") / "game_state_exports";
    std::filesystem::create_directories(dir, ec);
  }
  return dir;
}

// Write a JSON object to a file; returns true on success.
static bool write_json_file(const std::filesystem::path& path, const json& j)
{
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::trunc);
  if (!out.is_open()) {
    spdlog::error("GameState export: Failed to open {} for writing", path.string());
    return false;
  }
  out << j.dump(2);
  out.close();
  return true;
}

void export_game_state()
{
  try {
    auto& cfg   = Config::Get();
    auto  dir   = get_export_dir();
    auto  ts    = get_iso8601_timestamp();
    auto& gist  = cfg.game_state_github;
    const auto& o = cfg.game_state_options;

    json gs = build_game_state_json();

    // --- player.json (always written when game_state_enabled) ---
    {
      json j;
      j["exported_at"] = ts;
      j["player"]      = gs["player"];
      j["station"]     = gs["station"];
      j["drydocks"]    = gs["drydocks"];
      if (gs.contains("queues")) j["queues"] = gs["queues"];
      write_json_file(dir / "player.json", j);
    }

    // --- buildings.json ---
    if (o.buildings) {
      json j;
      j["exported_at"] = ts;
      j["buildings"]   = gs["buildings"];
      write_json_file(dir / "buildings.json", j);
    }

    // --- ships.json (ships + inventory flags) ---
    if (o.ships || o.inventory) {
      json j;
      j["exported_at"] = ts;
      if (o.ships)     { j["ships"]           = gs["ships"];           }
      if (o.resources) { j["blueprints"]       = gs["blueprints"];      }
      if (o.inventory) { j["ship_blueprints"]  = gs["ship_blueprints"]; }
      write_json_file(dir / "ships.json", j);
    }

    // --- resources.json ---
    if (o.resources) {
      json j;
      j["exported_at"] = ts;
      json res = json::array();
      for (const auto& r : gs["resources"]) {
        if (r.value("amount", 0LL) != 0) res.push_back(r);
      }
      j["resources"] = res;
      write_json_file(dir / "resources.json", j);
    }

    // --- research.json ---
    if (o.research) {
      json j;
      j["exported_at"] = ts;
      j["research"]    = gs["research"];
      write_json_file(dir / "research.json", j);
    }

    // --- officers.json ---
    if (o.officer) {
      json j;
      j["exported_at"] = ts;
      j["officers"]    = gs["officers"];
      write_json_file(dir / "officers.json", j);
    }

    // --- missions.json ---
    if (o.missions) {
      json j;
      j["exported_at"]        = ts;
      j["missions_active"]    = gs["missions_active"];
      j["missions_completed"] = gs["missions_completed"];
      write_json_file(dir / "missions.json", j);
    }

    // --- faction.json (resources + buffs + research all contribute) ---
    if (o.resources || o.buffs || o.research) {
      json j;
      j["exported_at"] = ts;
      if (o.resources) {
        j["faction_reputation"]   = gs["faction_reputation"];
        j["faction_store_tokens"] = gs["faction_store_tokens"];
        j["armada_credits"]       = gs["armada_credits"];
      }
      if (o.research && o.buffs) j["faction_favors"]         = gs["faction_favors"];
      if (o.buffs)                j["syndicate_loyalty_buffs"] = gs["syndicate_loyalty_buffs"];
      write_json_file(dir / "faction.json", j);
    }

    // --- buffs.json ---
    if (o.buffs) {
      json j;
      j["exported_at"]  = ts;
      j["buff_catalog"] = gs["buff_catalog"];
      write_json_file(dir / "buffs.json", j);
    }

    // --- territory.json (always written - no sync flag covers territory) ---
    {
      json j;
      j["exported_at"] = ts;
      j["territory"]   = gs["territory"];
      write_json_file(dir / "territory.json", j);
    }

    // --- manifest.json ---
    {
      json manifest;
      manifest["exported_at"]  = ts;
      manifest["mod_version"]  = VER_FILE_VERSION_STR;
      manifest["description"]  = "STFC Community Mod \u2014 game state export index";

      auto make_entry = [&](const std::string& file, const std::string& gist_name,
                             const std::string& desc,
                             const std::vector<std::string>& keys) {
        json e;
        e["file"]        = file;
        e["description"] = desc;
        e["keys"]        = keys;
        if (gist.enabled && !gist.gist_id.empty()) {
          const std::string user_segment = gist.username.empty() ? "" : gist.username + "/";
          e["url"] = "https://gist.githubusercontent.com/" +
                     user_segment + gist.gist_id + "/raw/" + gist_name;
        }
        return e;
      };

      manifest["files"] = json::array({
        make_entry("player.json",    gist.filename_player,
          "Player profile (name, ops level, power, server, syndicate level/XP), station (home system, last relocation, days in system, relocation tokens, peace shield), drydock assignments (with per-ship repair status), alliance (name, tag, level, member count, power, starbase_system_id), and active job queues (research, build, scrap)",
          {"player", "station", "drydocks", "queues"}),
        make_entry("buildings.json", gist.filename_buildings,
          "Station buildings (modules) with their current level",
          {"buildings"}),
        make_entry("ships.json",     gist.filename_ships,
          "Ships in the hangar (hull, tier, level, components, tier_max_level, tier_up_duration_secs, cargo_capacity, cargo_protection), blueprint part counts, and ship unlock blueprint progress",
          {"ships", "blueprints", "ship_blueprints"}),
        make_entry("resources.json", gist.filename_resources,
          "All non-zero resource amounts with names (raw materials, tokens, consumables, etc.)",
          {"resources"}),
        make_entry("research.json",  gist.filename_research,
          "All unlocked research nodes with their current level and tree name",
          {"research"}),
        make_entry("officers.json",  gist.filename_officers,
          "Officers with rank, level, shard count and all trait levels",
          {"officers"}),
        make_entry("missions.json",  gist.filename_missions,
          "Active mission instances and completed mission IDs",
          {"missions_active", "missions_completed"}),
        make_entry("faction.json",   gist.filename_faction,
          "Faction reputation points, store tokens, armada credits, per-faction store favors (all tiers, tier=0 means not yet purchased), and Syndicate Loyalty track buffs",
          {"faction_reputation", "faction_store_tokens", "armada_credits", "faction_favors", "syndicate_loyalty_buffs"}),
        make_entry("buffs.json",     gist.filename_buffs,
          "Full buff catalog from ShipBonusBuffSpecs: every buff with its modifier type, operation, per-level ranked values and faction affiliation where applicable",
          {"buff_catalog"}),
        make_entry("territory.json", gist.filename_territory,
          "Alliance territory holdings: held zones with tier and takeover windows, plus total/used slot counts",
          {"territory"}),
        make_entry("battlelog.json", gist.filename_battlelog,
          "Battle history (last 500 battles) with outcomes, ship IDs and resource changes",
          {"battlelog"}),
        // If a static site is present under community_patch/game_state_site, a copy will be
        // placed under the export dir as `site/` and uploaded to the gist. The site entry
        // here points to the index; other site assets are uploaded automatically.
        make_entry("site/index.html", "site/index.html",
          "Static LCARS web viewer (index.html)",
          {"site"}),
      });

      write_json_file(dir / "manifest.json", manifest);
    }

    // Sync all game state files to Gist in a single PATCH request
    // If the repo/game contains a static site to ship with the exports, copy it into
    // the export directory under `site/` so it will be considered for one-time upload.
    try {
      std::filesystem::path site_src = File::ExeDir() / "community_patch" / "game_state_site";
      std::filesystem::path site_dst = dir / "site";
      if (std::filesystem::exists(site_src)) {
        std::error_code ec;
        std::filesystem::create_directories(site_dst, ec);
        for (auto& entry : std::filesystem::recursive_directory_iterator(site_src)) {
          if (!entry.is_regular_file()) continue;
          auto rel = std::filesystem::relative(entry.path(), site_src);
          auto dstp = site_dst / rel;
          std::filesystem::create_directories(dstp.parent_path(), ec);
          std::filesystem::copy_file(entry.path(), dstp, std::filesystem::copy_options::overwrite_existing, ec);
          if (ec) spdlog::warn("GameState export: Failed to copy site asset {} -> {}: {}",
                              entry.path().string(), dstp.string(), ec.message());
        }
        spdlog::info("GameState export: Included static site from {} into exports", site_src.string());
      }
    } catch (const std::exception& e) {
      spdlog::warn("GameState export: Exception while copying site assets: {}", e.what());
    }

    // Perform a one-time upload of an inlined index.html containing CSS+JS if needed.
    try {
      sync_site_index_to_gist(dir);
    } catch (...) {}

    // Regular JSON files sync (runs as normal and won't include the site assets repeatedly)
    sync_all_to_gist(dir);

    last_full_export_time = ts;
    last_full_export_tp   = std::chrono::steady_clock::now();

    spdlog::info("GameState export: Exported all sections to {}/  (ships={} resources={} research={} officers={} missions_active={} missions_completed={})",
                 dir.string(),
                 gs["ships"].size(), gs["resources"].size(), gs["research"].size(),
                 gs["officers"].size(), gs["missions_active"].size(), gs["missions_completed"].size());

  } catch (const std::exception& e) {
    spdlog::error("GameState export: Exception during export: {}", e.what());
  }
}

static constexpr int MAX_BATTLELOG_ENTRIES = 500;

// Attempt to fill outcome/ship/location for any entries that were imported
// before the player name was known. Called from capture_player_data.
static void resolve_pending_battlelog_outcomes(const std::string& our_name,
                                               const std::string& out_path)
{
  std::unordered_set<std::string> still_pending;
  json entries = json::array();
  {
    std::ifstream in(out_path);
    if (!in.is_open()) return;
    try { in >> entries; if (!entries.is_array()) return; }
    catch (...) { return; }
  }

  bool any_resolved = false;
  for (auto& entry : entries) {
    const auto id = entry.value("id", "");
    {
      std::scoped_lock lk(pending_battlelog_mtx);
      if (!pending_battlelog_resolution.count(id)) continue;
    }
    // Try to resolve from the stored combatants array
    if (entry.contains("combatants") && entry["combatants"].is_array()) {
      for (const auto& row : entry["combatants"]) {
        if (row.value("Player Name", "") == our_name) {
          entry["outcome"]  = row.value("Outcome", "");
          entry["location"] = row.value("Location", "");
          entry["ship"]     = row.value("Ship Name", "");
          any_resolved = true;
          spdlog::info("Battlelog: resolved outcome for entry {} -> outcome={} ship={}",
                       id, entry["outcome"].get<std::string>(),
                       entry["ship"].get<std::string>());
          break;
        }
      }
    }
    // Whether or not we resolved it, remove from pending
    {
      std::scoped_lock lk(pending_battlelog_mtx);
      pending_battlelog_resolution.erase(id);
    }
  }

  if (!any_resolved) return;

  std::ofstream out(out_path, std::ios::trunc);
  if (!out.is_open()) { spdlog::error("Battlelog: Cannot rewrite {} for resolution", out_path); return; }
  out << entries.dump(2);
  out.close();
  sync_to_gist(out_path, Config::Get().game_state_github.filename_battlelog);
}

// Parse a tab-separated CSV section (header row + data rows) into a JSON array
static json parse_csv_section(const std::vector<std::string>& lines, size_t& pos)
{
  json result = json::array();
  if (pos >= lines.size() || lines[pos].empty()) return result;

  std::vector<std::string> headers;
  std::istringstream hss(lines[pos++]);
  std::string cell;
  while (std::getline(hss, cell, '\t')) headers.push_back(cell);

  while (pos < lines.size() && !lines[pos].empty()) {
    std::vector<std::string> values;
    std::istringstream vss(lines[pos++]);
    while (std::getline(vss, cell, '\t')) values.push_back(cell);
    json row = json::object();
    for (size_t i = 0; i < headers.size(); ++i)
      row[headers[i]] = (i < values.size()) ? values[i] : "";
    result.push_back(row);
  }
  return result;
}

static void process_battle_csv(const std::string& csv_path)
{
  auto& cfg = Config::Get();
  try {
    std::ifstream f(csv_path);
    if (!f.is_open()) { spdlog::warn("Battlelog CSV: Could not open {}", csv_path); return; }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(f, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      lines.push_back(line);
    }
    f.close();
    if (lines.empty()) return;

    // Four tab-separated sections divided by blank lines:
    // 0: combatants  1: rewards  2: fleet stats  3: round events
    size_t pos = 0;
    json combatants  = parse_csv_section(lines, pos);
    while (pos < lines.size() && lines[pos].empty()) ++pos;
    json rewards     = parse_csv_section(lines, pos);
    while (pos < lines.size() && lines[pos].empty()) ++pos;
    json fleet_stats = parse_csv_section(lines, pos);
    while (pos < lines.size() && lines[pos].empty()) ++pos;
    json rounds      = parse_csv_section(lines, pos);

    if (combatants.empty()) { spdlog::warn("Battlelog CSV: No combatant data in {}", csv_path); return; }

    const std::string filename = std::filesystem::path(csv_path).stem().string();
    json entry;
    entry["id"]          = filename;
    entry["source_file"] = std::filesystem::path(csv_path).filename().string();
    if (!combatants.empty() && combatants[0].contains("Timestamp"))
      entry["timestamp"] = combatants[0]["Timestamp"];

    // Find our player's outcome row
    std::string our_name;
    {
      std::scoped_lock lk(game_data_mutex);
      our_name = cached_player_data.value("name", "");
    }
    bool resolved = false;
    for (const auto& row : combatants) {
      if (!our_name.empty() && row.value("Player Name", "") == our_name) {
        entry["outcome"]  = row.value("Outcome", "");
        entry["location"] = row.value("Location", "");
        entry["ship"]     = row.value("Ship Name", "");
        resolved = true;
        break;
      }
    }
    if (!resolved) {
      // Name not yet known â€” mark for resolution when capture_player_data fires
      std::scoped_lock lk(pending_battlelog_mtx);
      pending_battlelog_resolution.insert(filename);
      spdlog::info("Battlelog: outcome unresolved for {} (player name not yet known), will retry", filename);
    }

    entry["combatants"]  = combatants;
    entry["rewards"]     = rewards;
    entry["fleet_stats"] = fleet_stats;
    entry["rounds"]      = rounds;

    const auto out_path = (get_export_dir() / "battlelog.json").string();

    json entries = json::array();
    std::ifstream in(out_path);
    if (in.is_open()) {
      try { in >> entries; if (!entries.is_array()) entries = json::array(); }
      catch (...) { entries = json::array(); }
      in.close();
    }

    for (const auto& e : entries) {
      if (e.value("id", "") == filename) { spdlog::debug("Battlelog CSV: duplicate {}", filename); return; }
    }

    entries.push_back(entry);
    while (entries.size() > MAX_BATTLELOG_ENTRIES) entries.erase(entries.begin());

    std::ofstream out(out_path, std::ios::trunc);
    if (!out.is_open()) { spdlog::error("Battlelog CSV: Cannot write {}", out_path); return; }
    out << entries.dump(2);
    out.close();

    spdlog::info("Battlelog CSV: Imported {} ({} total entries)", filename, entries.size());
    sync_to_gist(out_path, cfg.game_state_github.filename_battlelog);

  } catch (const std::exception& e) {
    spdlog::error("Battlelog CSV: Exception processing {}: {}", csv_path, e.what());
  }
}

// --- Peace shield warning state ---
static std::chrono::steady_clock::time_point last_shield_down_warn_tp;
static std::chrono::steady_clock::time_point last_shield_expiry_warn_tp;
static int  last_shield_expiry_threshold_hours = -1; // which threshold fired last

static void check_shield_warnings()
{
  const auto& cfg = Config::Get();
  if (!cfg.shield_alerts.enabled) return;

  auto now_steady = std::chrono::steady_clock::now();
  auto now_epoch  = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();

  int64_t expiry_epoch;
  int64_t token_count;
  bool    scan_received;
  {
    std::scoped_lock lock(game_data_mutex);
    expiry_epoch  = cached_shield_expiry_epoch;
    token_count   = cached_shield_token_count;
    scan_received = shield_scan_received;
  }

  // Don't warn until we've received at least one StarbaseDetailedScan for
  // the player's own station â€” we simply don't know the shield state yet.
  if (!scan_received) return;

  const auto reminder_dur =
      std::chrono::minutes(cfg.shield_alerts.reminder_interval_minutes);

  bool shield_active = expiry_epoch > now_epoch;

  if (!shield_active) {
    // Warn that shield is down, respecting reminder interval
    auto since_last = now_steady - last_shield_down_warn_tp;
    if (last_shield_down_warn_tp == std::chrono::steady_clock::time_point{}) {
      // First confirmed reading with no active shield - may just not have navigated
      // to trigger StarbaseDetailedScan yet; note that in the log rather than alarming.
      spdlog::info("SHIELD ALERT: Station peace shield confirmed DOWN (first reading). "
                   "If this is unexpected, navigate in-game to your station to re-confirm. "
                   "Tokens in inventory: {}", token_count);
      last_shield_down_warn_tp = now_steady;
    } else if (since_last >= reminder_dur) {
      spdlog::warn("SHIELD ALERT: Station peace shield is DOWN. Tokens in inventory: {}",
                   token_count);
      last_shield_down_warn_tp = now_steady;
    }
    // Reset expiry threshold tracking when shield is down
    last_shield_expiry_threshold_hours = -1;
  } else {
    // Reset the "shield down" warning timer now that the shield is up
    last_shield_down_warn_tp = std::chrono::steady_clock::time_point{};

    // Check each configured expiry threshold (sorted descending so largest fires first)
    auto thresholds = cfg.shield_alerts.shield_warn_hours;
    std::sort(thresholds.begin(), thresholds.end(), std::greater<int>());

    int64_t seconds_remaining = expiry_epoch - now_epoch;
    for (int hours : thresholds) {
      int64_t threshold_secs = static_cast<int64_t>(hours) * 3600;
      if (seconds_remaining <= threshold_secs) {
        // Only fire this threshold if we haven't already warned for it
        // (or a smaller one) this cycle, and respect reminder interval
        bool new_threshold = (last_shield_expiry_threshold_hours == -1 ||
                              hours < last_shield_expiry_threshold_hours);
        bool reminder_due  = (last_shield_expiry_warn_tp == std::chrono::steady_clock::time_point{} ||
                              (now_steady - last_shield_expiry_warn_tp) >= reminder_dur);
        if (new_threshold || reminder_due) {
          int hrs  = static_cast<int>(seconds_remaining / 3600);
          int mins = static_cast<int>((seconds_remaining % 3600) / 60);
          spdlog::warn("SHIELD ALERT: Station peace shield expires in {}h {}m (threshold: {}h)",
                       hrs, mins, hours);
          last_shield_expiry_warn_tp         = now_steady;
          last_shield_expiry_threshold_hours = hours;
        }
        break; // only fire the innermost (smallest) breached threshold
      }
    }
  }
}

void export_thread_func()
{
  auto& cfg = Config::Get();

  // Track CSV files we've already processed
  std::unordered_set<std::string> processed_csvs;

  // Pre-populate with any CSVs already on disk so we don't re-import old ones
  // prime_Data sits alongside the game executable â€” derive from game_state_path if set,
  // otherwise fall back to the game module's own directory.
  std::filesystem::path game_dir;
  if (!cfg.game_state_path.empty()) {
    game_dir = std::filesystem::path(cfg.game_state_path);
  } else {
#if _WIN32
    wchar_t module_path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, module_path, MAX_PATH);
    game_dir = std::filesystem::path(module_path).parent_path();
#else
    game_dir = std::filesystem::current_path();
#endif
  }
  const std::filesystem::path csv_dir = game_dir / "prime_Data";
  if (cfg.game_state_battle_log) {
    spdlog::info("Battlelog CSV watcher: enabled, watching {}", csv_dir.string());
    if (std::filesystem::exists(csv_dir)) {
      for (const auto& entry : std::filesystem::directory_iterator(csv_dir)) {
        if (entry.path().extension() == ".csv") {
          processed_csvs.insert(entry.path().string());
        }
      }
      spdlog::info("Battlelog CSV watcher: pre-seeded {} existing CSV files", processed_csvs.size());
    } else {
      spdlog::warn("Battlelog CSV watcher: directory not found: {}", csv_dir.string());
    }
  } else {
    spdlog::info("Battlelog CSV watcher: disabled (set battle_log = true in [sync.game_state] to enable)");
  }

  spdlog::info("GameState export thread started");

  // Always export full snapshot on startup, but wait for grace period first
  if (cfg.game_state_on_startup) {
    spdlog::info("GameState export: Waiting {} seconds for all game data to load before startup export", 
                 STARTUP_GRACE_PERIOD_SECONDS);
    std::this_thread::sleep_for(std::chrono::seconds(STARTUP_GRACE_PERIOD_SECONDS));
    spdlog::info("GameState export: Performing startup full export with all captured data");
    export_game_state();
    // Clear any requests that queued up during the grace period and reset the
    // debounce clock so the post-startup data bursts don't fire again immediately.
    {
      std::scoped_lock lock(export_request_mutex);
      should_export_now      = false;
      last_export_request_tp = std::chrono::steady_clock::now();
    }
  }

  auto last_export_time = std::chrono::steady_clock::now();

  while (!should_stop) {
    // Wait for either immediate export request, interval timeout, or CSV poll (10s)
    std::unique_lock<std::mutex> lock(export_request_mutex);

    if (cfg.game_state_interval > 0) {
      export_cv.wait_for(lock, std::chrono::seconds(std::min(cfg.game_state_interval, 10)), 
                         [] { return should_export_now || should_stop; });
    } else {
      export_cv.wait_for(lock, std::chrono::seconds(10),
                         [] { return should_export_now || should_stop; });
    }

    if (should_stop) {
      break;
    }

    auto now = std::chrono::steady_clock::now();

    // Export if explicitly requested or interval has elapsed
    auto seconds_since_last_export = std::chrono::duration_cast<std::chrono::seconds>(now - last_export_time).count();
    bool interval_elapsed = cfg.game_state_interval > 0 && seconds_since_last_export >= cfg.game_state_interval;

    if (should_export_now || interval_elapsed) {
      // Debounce: wait until no new request has arrived for EXPORT_DEBOUNCE_SECONDS
      // so that a burst of captures (e.g. buildings arriving in batches) produces
      // one export rather than N back-to-back exports.
      auto time_since_last_request = std::chrono::duration_cast<std::chrono::seconds>(
          now - last_export_request_tp).count();
      if (should_export_now && !interval_elapsed &&
          time_since_last_request < EXPORT_DEBOUNCE_SECONDS) {
        lock.unlock();
        // Don't clear should_export_now â€” re-check on next loop iteration
      } else {
        should_export_now = false;
        lock.unlock();
        export_game_state();
        last_export_time = now;
      }
    } else {
      lock.unlock();
    }

    // Poll prime_Data/ for new battle CSV files (only if battle_log enabled)
    if (cfg.game_state_battle_log && std::filesystem::exists(csv_dir)) {
      for (const auto& entry : std::filesystem::directory_iterator(csv_dir)) {
        if (entry.path().extension() != ".csv") continue;
        const std::string csv_path = entry.path().string();
        if (processed_csvs.count(csv_path)) continue;
        processed_csvs.insert(csv_path);
        process_battle_csv(csv_path);
      }
    }

    // Peace shield / resource alerts poll
    {
      const auto& alert_cfg = Config::Get().shield_alerts;
      if (alert_cfg.enabled) {
        static std::chrono::steady_clock::time_point last_alert_check;
        auto now = std::chrono::steady_clock::now();
        auto poll_dur = std::chrono::seconds(alert_cfg.poll_interval_seconds);
        if (last_alert_check == std::chrono::steady_clock::time_point{} ||
            (now - last_alert_check) >= poll_dur) {
          check_shield_warnings();
          last_alert_check = now;
        }
      }
    }
  }

  spdlog::info("GameState export thread stopped");
}

void init()
{
  auto& cfg = Config::Get();

  if (!cfg.game_state_enabled) {
    spdlog::info("GameState export is disabled in config");
    return;
  }

  spdlog::info("GameState export: Enabled with interval={} seconds, path='{}', on_startup={}",
               cfg.game_state_interval,
               cfg.game_state_path.empty() ? "<game directory>" : cfg.game_state_path,
               cfg.game_state_on_startup);

  // Log which data types are active (set to false in [sync] to suppress)
  const auto& o = cfg.game_state_options;
  spdlog::info("GameState export: active types - ships={} research={} buildings={} resources={} "
               "officer={} traits={} missions={} buffs={} inventory={} battle_log={}",
               o.ships, o.research, o.buildings, o.resources,
               o.officer, o.traits, o.missions, o.buffs, o.inventory,
               cfg.game_state_battle_log);

  if (cfg.game_state_github.enabled) {
    auto& gist = cfg.game_state_github;
    const std::string user_segment = gist.username.empty() ? "" : gist.username + "/";
    spdlog::info("GameState Gist sync: Enabled for gist_id={}{}", gist.gist_id,
                 gist.username.empty() ? " (username not set â€” URLs in manifest may be incomplete)" : "");
    spdlog::info("GameState Gist sync: Manifest URL: https://gist.githubusercontent.com/{}{}raw/{}",
                 user_segment, gist.gist_id + "/", gist.filename_manifest);
    for (const auto* fn : {&gist.filename_player, &gist.filename_ships, &gist.filename_resources,
                            &gist.filename_research, &gist.filename_officers, &gist.filename_missions,
                            &gist.filename_faction, &gist.filename_buffs, &gist.filename_territory,
                            &gist.filename_battlelog}) {
      spdlog::info("GameState Gist sync:   https://gist.githubusercontent.com/{}{}raw/{}", user_segment, gist.gist_id + "/", *fn);
    }
  } else {
    spdlog::info("GameState Gist sync: Disabled (set [gamestate_export.gist] enabled=true to activate)");
  }

  // Record startup time for grace period
  startup_time = std::chrono::steady_clock::now();

  // Load ID mappings â€” anchor to the exe directory so this works regardless
  // of what the process working directory happens to be.
  // Look for mappings in community_patch/game_data_maps/ first, fall back to legacy game_data_maps/
  std::filesystem::path mappings_path = File::ExeDir() / "community_patch" / "game_data_maps" / "stfc_id_mappings.json";
  if (!std::filesystem::exists(mappings_path)) {
    mappings_path = File::ExeDir() / "game_data_maps" / "stfc_id_mappings.json";
    if (std::filesystem::exists(mappings_path)) {
      spdlog::warn("GameState export: ID mappings found at legacy path {}. Consider moving to community_patch/game_data_maps/",
                   mappings_path.string());
    }
  }
  if (std::filesystem::exists(mappings_path)) {
    id_mappings::MappingCache::Get().load_mappings(mappings_path.string());
  } else {
    spdlog::warn("GameState export: ID mappings file not found. Exported JSON will only contain IDs.");
  }

  // Load system name lookup (stfc_systems.json) for territory name derivation.
  // File is optional — territory names will simply be absent if not found.
  std::filesystem::path systems_path = File::ExeDir() / "community_patch" / "game_data_maps" / "stfc_systems.json";
  spdlog::debug("GameState export: looking for stfc_systems.json at {}", systems_path.string());
  if (std::filesystem::exists(systems_path)) {
    try {
      std::ifstream sf(systems_path.string());
      if (!sf.is_open()) {
        spdlog::warn("GameState export: stfc_systems.json exists but could not be opened");
      } else {
        nlohmann::json sys_data;
        sf >> sys_data;
        for (const auto& [id_str, name] : sys_data.items()) {
          if (name.is_string()) {
            cached_system_names[std::stoll(id_str)] = name.get<std::string>();
          }
        }
        spdlog::info("GameState export: loaded {} system names from stfc_systems.json", cached_system_names.size());
      }
    } catch (const std::exception& e) {
      spdlog::warn("GameState export: failed to load stfc_systems.json: {}", e.what());
    }
  } else {
    spdlog::warn("GameState export: stfc_systems.json not found at {}", systems_path.string());
  }

  should_stop = false;
  export_thread = std::thread(export_thread_func);
}

void export_now()
{
  export_game_state();
}

} // namespace game_state_export
