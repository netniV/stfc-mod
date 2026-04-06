#include "gamestate_export.h"
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

namespace gamestate_export
{

static std::thread export_thread;
static bool        should_stop = false;
static bool        should_export_now = false;
static bool        should_force_full_export = false;
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

// Drydock assignments: drydock_id (1-based) -> player ship id
static std::vector<std::pair<int32_t, int64_t>> cached_drydock_assignments;

// Previous state for differential tracking
static std::unordered_map<int64_t, int64_t> previous_resources;
static std::unordered_map<int64_t, int32_t> previous_buildings;
static std::unordered_map<int64_t, json> previous_ships;
static std::unordered_map<int64_t, int32_t> previous_research;
static std::unordered_map<uint64_t, json> previous_officers;

// Change tracking
static std::string last_full_export_time;
static std::string last_differential_export_time;
static std::chrono::steady_clock::time_point last_full_export_tp;
static std::chrono::steady_clock::time_point last_differential_export_tp;
static std::chrono::steady_clock::time_point startup_time;
static constexpr int STARTUP_GRACE_PERIOD_SECONDS = 15; // Wait 15 seconds after startup before first export

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

  std::scoped_lock lock(export_request_mutex);
  should_export_now = true;
  export_cv.notify_one();
}

static void request_full_export()
{
  auto now = std::chrono::steady_clock::now();
  auto seconds_since_startup = std::chrono::duration_cast<std::chrono::seconds>(now - startup_time).count();
  if (seconds_since_startup < STARTUP_GRACE_PERIOD_SECONDS) return;

  std::scoped_lock lock(export_request_mutex);
  should_force_full_export = true;
  should_export_now = true;
  export_cv.notify_one();
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
    request_full_export();
  }
}

void capture_player_alliance(const std::string& alliance_name, const std::string& alliance_tag)
{
  std::scoped_lock lock(game_data_mutex);
  if (!cached_player_data.is_null()) {
    cached_player_data["alliance"] = alliance_name + " [" + alliance_tag + "]";
  }
}

void capture_resources(const nlohmann::json& data)
{
  std::scoped_lock lock(game_data_mutex);
  for (const auto& [str_id, resource] : data.items()) {
    auto id = std::stoll(str_id);
    auto amount = resource["current_amount"].get<int64_t>();
    cached_resources[id] = amount;
  }
  request_immediate_export();
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
  std::scoped_lock lock(game_data_mutex);
  cached_shield_expiry_epoch = active_expiry_epoch;
  cached_shield_token_count  = token_count;
  spdlog::info("GameState: Captured peace shield expiry={}, tokens={}",
               active_expiry_epoch, token_count);
  request_immediate_export();
}

void capture_drydock_assignments(const std::vector<std::pair<int32_t, int64_t>>& assignments)
{
  std::scoped_lock lock(game_data_mutex);
  cached_drydock_assignments = assignments;
  spdlog::info("GameState: Captured {} drydock assignments", assignments.size());
  request_immediate_export();
}

// Helper function to calculate change percentage
static double calculate_change_percentage()
{
  size_t total_items = cached_buildings.size() + cached_research.size() + 
                       cached_ships.size() + cached_officers.size() + cached_resources.size();
  size_t changed_items = 0;

  // Count changed buildings
  for (const auto& [id, level] : cached_buildings) {
    if (previous_buildings.find(id) == previous_buildings.end() || previous_buildings[id] != level) {
      changed_items++;
    }
  }

  // Count changed research
  for (const auto& [id, level] : cached_research) {
    if (previous_research.find(id) == previous_research.end() || previous_research[id] != level) {
      changed_items++;
    }
  }

  // Count changed ships
  for (const auto& [id, data] : cached_ships) {
    if (previous_ships.find(id) == previous_ships.end() || previous_ships[id] != data) {
      changed_items++;
    }
  }

  // Count changed officers
  for (const auto& [id, data] : cached_officers) {
    if (previous_officers.find(id) == previous_officers.end() || previous_officers[id] != data) {
      changed_items++;
    }
  }

  // Count changed resources
  for (const auto& [id, amount] : cached_resources) {
    if (previous_resources.find(id) == previous_resources.end() || previous_resources[id] != amount) {
      changed_items++;
    }
  }

  return total_items > 0 ? (static_cast<double>(changed_items) / total_items) : 0.0;
}

// Build differential export JSON
json build_differential_json()
{
  std::scoped_lock lock(game_data_mutex);

  json j;
  j["export_type"] = "differential";
  j["export_version"] = "1.1.0";
  j["exported_at"] = get_iso8601_timestamp();
  j["base_export_at"] = last_full_export_time;
  j["mod_version"] = VER_FILE_VERSION_STR;

  json changes;
  int total_changes = 0;
  std::vector<std::string> categories_changed;

  // Buildings changes
  json buildings_changes;
  buildings_changes["modified"] = json::array();
  buildings_changes["added"] = json::array();
  buildings_changes["removed"] = json::array();

  for (const auto& [id, level] : cached_buildings) {
    auto prev_it = previous_buildings.find(id);
    if (prev_it == previous_buildings.end()) {
      // New building
      json building = {{"id", id}, {"level", level}};
      id_mappings::MappingCache::Get().enrich_building(building);
      buildings_changes["added"].push_back(building);
      total_changes++;
    } else if (prev_it->second != level) {
      // Modified building
      json building = {{"id", id}, {"level", level}, {"previous_level", prev_it->second}};
      id_mappings::MappingCache::Get().enrich_building(building);
      buildings_changes["modified"].push_back(building);
      total_changes++;
    }
  }

  // Check for removed buildings
  for (const auto& [id, level] : previous_buildings) {
    if (cached_buildings.find(id) == cached_buildings.end()) {
      json building = {{"id", id}, {"level", level}};
      id_mappings::MappingCache::Get().enrich_building(building);
      buildings_changes["removed"].push_back(building);
      total_changes++;
    }
  }

  if (total_changes > 0) {
    changes["buildings"] = buildings_changes;
    categories_changed.push_back("buildings");
  }

  // Research changes
  json research_changes;
  research_changes["modified"] = json::array();
  research_changes["added"] = json::array();

  int research_changed = 0;
  for (const auto& [id, level] : cached_research) {
    auto prev_it = previous_research.find(id);
    if (prev_it == previous_research.end()) {
      // New research
      json research = {{"id", id}, {"level", level}};
      id_mappings::MappingCache::Get().enrich_research(research);
      research_changes["added"].push_back(research);
      research_changed++;
    } else if (prev_it->second != level) {
      // Modified research
      json research = {{"id", id}, {"level", level}, {"previous_level", prev_it->second}};
      id_mappings::MappingCache::Get().enrich_research(research);
      research_changes["modified"].push_back(research);
      research_changed++;
    }
  }

  if (research_changed > 0) {
    changes["research"] = research_changes;
    categories_changed.push_back("research");
    total_changes += research_changed;
  }

  // Resource changes
  json resources_changes;
  int resources_changed = 0;

  for (const auto& [id, amount] : cached_resources) {
    auto prev_it = previous_resources.find(id);
    if (prev_it != previous_resources.end() && prev_it->second != amount) {
      json resource = {
        {"id", id},
        {"current", amount},
        {"previous", prev_it->second},
        {"delta", amount - prev_it->second}
      };
      id_mappings::MappingCache::Get().enrich_resource(resource, id);
      resources_changes[std::to_string(id)] = resource;
      resources_changed++;
    }
  }

  if (resources_changed > 0) {
    changes["resources"] = resources_changes;
    categories_changed.push_back("resources");
    total_changes += resources_changed;
  }

  // Summary
  j["changes"] = changes;
  j["summary"] = {
    {"total_changes", total_changes},
    {"categories_changed", categories_changed}
  };

  return j;
}

// Update previous state snapshots
static void update_previous_state()
{
  previous_buildings = cached_buildings;
  previous_research = cached_research;
  previous_ships = cached_ships;
  previous_officers = cached_officers;
  previous_resources = cached_resources;
}

json build_gamestate_json()
{
  std::scoped_lock lock(game_data_mutex);

  json j;

  j["meta"]["version"] = "1.0.0";
  j["meta"]["exported_at"] = get_iso8601_timestamp();
  j["meta"]["mod_version"] = VER_FILE_VERSION_STR;
  j["meta"]["mappings_loaded"] = id_mappings::MappingCache::Get().is_loaded();

  // Player info
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

  // Buildings - enrich with names
  j["buildings"] = json::array();
  for (const auto& [id, level] : cached_buildings) {
    json building = {
      {"id", id},
      {"level", level}
    };
    id_mappings::MappingCache::Get().enrich_building(building);
    j["buildings"].push_back(building);
  }

  // Research - enrich with names
  j["research"] = json::array();
  for (const auto& [id, level] : cached_research) {
    json research = {
      {"id", id},
      {"level", level}
    };
    id_mappings::MappingCache::Get().enrich_research(research);
    j["research"].push_back(research);
  }

  // Ships - enrich with names
  j["ships"] = json::array();
  for (const auto& [id, ship_data] : cached_ships) {
    json ship_entry = ship_data;
    ship_entry["id"] = id;
    id_mappings::MappingCache::Get().enrich_ship(ship_entry);
    j["ships"].push_back(ship_entry);
  }

  // Officers - enrich with names and inject trait ability levels
  j["officers"] = json::array();
  for (const auto& [id, officer_data] : cached_officers) {
    json officer_entry = officer_data;
    officer_entry["id"] = id;
    id_mappings::MappingCache::Get().enrich_officer(officer_entry);

    // Inject player's trait ability levels into the traits array
    auto it = cached_officer_traits.find(id);
    if (it != cached_officer_traits.end() && officer_entry.contains("traits")) {
      for (auto& trait : officer_entry["traits"]) {
        uint64_t trait_id = trait["id"].get<uint64_t>();
        auto level_it = it->second.find(trait_id);
        trait["ability_level"] = (level_it != it->second.end()) ? level_it->second : 0;
      }
    }

    j["officers"].push_back(officer_entry);
  }

  // Resources - enrich with names
  j["resources"] = json::array();
  for (const auto& [id, amount] : cached_resources) {
    json resource = {
      {"id", id},
      {"amount", amount}
    };
    id_mappings::MappingCache::Get().enrich_resource(resource, id);
    j["resources"].push_back(resource);
  }

  // Faction reputation - extracted from resources named Resource_FactionPoint_*
  j["faction_reputation"] = json::array();
  for (const auto& [id, amount] : cached_resources) {
    auto mapping = id_mappings::MappingCache::Get().get_resource(id);
    if (!mapping) continue;
    const std::string& name = mapping->name;
    // Match resource names like "Resource_FactionPoint_Federation"
    const std::string prefix = "Resource_FactionPoint_";
    if (name.rfind(prefix, 0) == 0) {
      std::string faction = name.substr(prefix.size());
      j["faction_reputation"].push_back({
        {"faction", faction},
        {"resource_id", id},
        {"points", amount}
      });
    }
  }

  // Blueprint parts - resources named Resource_Parts_* or Resource_*_Parts_*
  j["blueprints"] = json::array();
  for (const auto& [id, amount] : cached_resources) {
    auto mapping = id_mappings::MappingCache::Get().get_resource(id);
    if (!mapping) continue;
    const std::string& name = mapping->name;
    if (name.find("_Parts_") != std::string::npos || name.find("_Parts") == name.size() - 6) {
      j["blueprints"].push_back({
        {"resource_id", id},
        {"name", name},
        {"amount", amount}
      });
    }
  }

  // Station peace shield
  {
    auto now_epoch = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    bool shield_active = cached_shield_expiry_epoch > now_epoch;

    json shield_json;
    shield_json["active"]        = shield_active;
    shield_json["token_count"]   = cached_shield_token_count;
    if (shield_active) {
      // Format expiry as ISO-8601
      std::time_t expiry_t = static_cast<std::time_t>(cached_shield_expiry_epoch);
      std::tm expiry_tm    = {};
#ifdef _WIN32
      gmtime_s(&expiry_tm, &expiry_t);
#else
      gmtime_r(&expiry_t, &expiry_tm);
#endif
      std::ostringstream oss;
      oss << std::put_time(&expiry_tm, "%Y-%m-%dT%H:%M:%SZ");
      shield_json["expires_at"]       = oss.str();
      shield_json["expires_epoch"]    = cached_shield_expiry_epoch;
      shield_json["seconds_remaining"] = cached_shield_expiry_epoch - now_epoch;
    } else {
      shield_json["expires_at"]        = nullptr;
      shield_json["expires_epoch"]     = nullptr;
      shield_json["seconds_remaining"] = 0;
    }
    j["station"]["peace_shield"] = shield_json;
  }

  // Drydock assignments — letter derived from 1-based drydock_id (1=A … 5=E)
  j["drydocks"] = json::array();
  for (const auto& [drydock_id, ship_id] : cached_drydock_assignments) {
    std::string letter(1, static_cast<char>('A' + drydock_id - 1));
    json entry;
    entry["drydock_id"] = drydock_id;
    entry["letter"]     = letter;
    entry["ship_id"]    = ship_id;
    // Enrich with ship name via hull_id lookup if the ship is in the hangar cache
    auto ship_it = cached_ships.find(ship_id);
    if (ship_it != cached_ships.end() && ship_it->second.contains("hull_id")) {
      int64_t hull_id = ship_it->second["hull_id"].get<int64_t>();
      auto mapping = id_mappings::MappingCache::Get().get_ship(hull_id);
      if (mapping) {
        entry["ship_name"] = mapping->name;
      }
    }
    j["drydocks"].push_back(entry);
  }
  // Annotate each ship in the ships array with its drydock letter
  for (auto& ship_entry : j["ships"]) {
    int64_t sid = ship_entry["id"].get<int64_t>();
    for (const auto& [drydock_id, ship_id] : cached_drydock_assignments) {
      if (ship_id == sid) {
        std::string letter(1, static_cast<char>('A' + drydock_id - 1));
        ship_entry["drydock"]    = letter;
        ship_entry["drydock_id"] = drydock_id;
        break;
      }
    }
  }

  spdlog::info("GameState JSON structure built: {} buildings, {} research, {} ships, {} officers, {} resources, {} drydocks",
                cached_buildings.size(), cached_research.size(), cached_ships.size(),
                cached_officers.size(), cached_resources.size(), cached_drydock_assignments.size());

  return j;
}

static void sync_to_gist(const std::string& file_path, const std::string& gist_filename)
{
  const auto& gist = Config::Get().export_gamestate_gist;
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

void export_gamestate()
{
  try {
    auto& cfg = Config::Get();

    std::string export_path;
    if (cfg.export_gamestate_path.empty()) {
      export_path = "community_patch_gamestate.json";
    } else {
      std::filesystem::path export_dir(cfg.export_gamestate_path);
      if (!std::filesystem::exists(export_dir)) {
        std::error_code ec;
        std::filesystem::create_directories(export_dir, ec);
        if (ec) {
          spdlog::error("GameState export: Failed to create directory {}: {}", cfg.export_gamestate_path, ec.message());
          export_path = "community_patch_gamestate.json";
        } else {
          export_path = (export_dir / "community_patch_gamestate.json").string();
        }
      } else {
        export_path = (export_dir / "community_patch_gamestate.json").string();
      }
    }

    json gamestate = build_gamestate_json();

    // Add export_type metadata
    gamestate["export_type"] = "full";
    gamestate["export_version"] = "1.1.0";

    std::ofstream out(export_path);
    if (!out.is_open()) {
      spdlog::error("GameState export: Failed to open file for writing: {}", export_path);
      return;
    }

    out << gamestate.dump(2);
    out.close();

    // Update tracking
    last_full_export_time = gamestate["meta"]["exported_at"];
    last_full_export_tp = std::chrono::steady_clock::now();
    update_previous_state();

    // Clear the differential file since we just did a full export
    std::string delta_path;
    if (cfg.export_gamestate_path.empty()) {
      delta_path = "community_patch_gamestate_delta.json";
    } else {
      delta_path = (std::filesystem::path(cfg.export_gamestate_path) / "community_patch_gamestate_delta.json").string();
    }

    // Create empty array for deltas
    std::ofstream delta_out(delta_path, std::ios::trunc);
    if (delta_out.is_open()) {
      delta_out << "[]";
      delta_out.close();
      spdlog::debug("GameState export: Cleared delta file for new baseline");
    }

    spdlog::info("GameState export: Successfully exported FULL snapshot to {}", export_path);
    sync_to_gist(export_path, Config::Get().export_gamestate_gist.filename_full);

  } catch (const std::exception& e) {
    spdlog::error("GameState export: Exception during export: {}", e.what());
  }
}

void export_differential()
{
  try {
    auto& cfg = Config::Get();

    // Need a baseline full export first
    if (last_full_export_time.empty()) {
      spdlog::warn("GameState differential export: No baseline full export exists, exporting full instead");
      export_gamestate();
      return;
    }

    std::string export_path;
    if (cfg.export_gamestate_path.empty()) {
      export_path = "community_patch_gamestate_delta.json";
    } else {
      std::filesystem::path export_dir(cfg.export_gamestate_path);
      if (!std::filesystem::exists(export_dir)) {
        std::error_code ec;
        std::filesystem::create_directories(export_dir, ec);
        if (ec) {
          spdlog::error("GameState export: Failed to create directory {}: {}", cfg.export_gamestate_path, ec.message());
          export_path = "community_patch_gamestate_delta.json";
        } else {
          export_path = (export_dir / "community_patch_gamestate_delta.json").string();
        }
      } else {
        export_path = (export_dir / "community_patch_gamestate_delta.json").string();
      }
    }

    json differential = build_differential_json();

    // Check if there are actually any changes
    {
      int total_changes = differential["summary"]["total_changes"];
      if (total_changes == 0) {
        spdlog::debug("GameState export: No changes detected, skipping differential export");
        return;
      }
    }

    // Read existing deltas array (if file exists)
    json deltas_array = json::array();
    std::ifstream in(export_path);
    if (in.is_open()) {
      try {
        in >> deltas_array;
        if (!deltas_array.is_array()) {
          spdlog::warn("GameState differential export: Existing delta file was not an array, recreating");
          deltas_array = json::array();
        }
      } catch (const std::exception& e) {
        spdlog::warn("GameState differential export: Failed to parse existing delta file: {}, recreating", e.what());
        deltas_array = json::array();
      }
      in.close();
    }

    // Append new differential to array
    deltas_array.push_back(differential);

    // Write updated array back
    std::ofstream out(export_path, std::ios::trunc);
    if (!out.is_open()) {
      spdlog::error("GameState differential export: Failed to open file for writing: {}", export_path);
      return;
    }

    out << deltas_array.dump(2);
    out.close();

    // Update tracking
    last_differential_export_time = differential["exported_at"];
    last_differential_export_tp = std::chrono::steady_clock::now();
    update_previous_state();

    int total_changes = differential["summary"]["total_changes"];
    spdlog::info("GameState export: Successfully APPENDED DIFFERENTIAL with {} changes ({} total deltas in file)", 
                 total_changes, deltas_array.size());
    sync_to_gist(export_path, Config::Get().export_gamestate_gist.filename_delta);

  } catch (const std::exception& e) {
    spdlog::error("GameState differential export: Exception during export: {}", e.what());
  }
}

static constexpr int MAX_BATTLELOG_ENTRIES = 500;

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
    for (const auto& row : combatants) {
      if (!our_name.empty() && row.value("Player Name", "") == our_name) {
        entry["outcome"]  = row.value("Outcome", "");
        entry["location"] = row.value("Location", "");
        entry["ship"]     = row.value("Ship Name", "");
        break;
      }
    }

    entry["combatants"]  = combatants;
    entry["rewards"]     = rewards;
    entry["fleet_stats"] = fleet_stats;
    entry["rounds"]      = rounds;

    std::string out_path;
    if (cfg.export_gamestate_path.empty()) {
      out_path = "community_patch_battlelog.json";
    } else {
      out_path = (std::filesystem::path(cfg.export_gamestate_path) / "community_patch_battlelog.json").string();
    }

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
    sync_to_gist(out_path, cfg.export_gamestate_gist.filename_battlelog);

  } catch (const std::exception& e) {
    spdlog::error("Battlelog CSV: Exception processing {}: {}", csv_path, e.what());
  }
}

// --- Peace shield warning state ---
static std::chrono::steady_clock::time_point last_shield_down_warn_tp;
static std::chrono::steady_clock::time_point last_shield_expiry_warn_tp;
static int last_shield_expiry_threshold_hours = -1; // which threshold fired last

static void check_shield_warnings()
{
  const auto& cfg = Config::Get();
  if (!cfg.resource_alerts.enabled) return;

  auto now_steady = std::chrono::steady_clock::now();
  auto now_epoch  = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();

  int64_t expiry_epoch;
  int64_t token_count;
  {
    std::scoped_lock lock(game_data_mutex);
    expiry_epoch = cached_shield_expiry_epoch;
    token_count  = cached_shield_token_count;
  }

  const auto reminder_dur =
      std::chrono::minutes(cfg.resource_alerts.reminder_interval_minutes);

  bool shield_active = expiry_epoch > now_epoch;

  if (!shield_active) {
    // Warn that shield is down, respecting reminder interval
    auto since_last = now_steady - last_shield_down_warn_tp;
    if (last_shield_down_warn_tp == std::chrono::steady_clock::time_point{} ||
        since_last >= reminder_dur) {
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
    auto thresholds = cfg.resource_alerts.shield_warn_hours;
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
  // prime_Data sits alongside the game executable — derive from export_gamestate_path if set,
  // otherwise fall back to the game module's own directory.
  std::filesystem::path game_dir;
  if (!cfg.export_gamestate_path.empty()) {
    game_dir = std::filesystem::path(cfg.export_gamestate_path);
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
  spdlog::info("Battlelog CSV watcher: watching {}", csv_dir.string());
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

  spdlog::info("GameState export thread started");

  // Always export full snapshot on startup, but wait for grace period first
  if (cfg.export_gamestate_on_startup) {
    spdlog::info("GameState export: Waiting {} seconds for all game data to load before startup export", 
                 STARTUP_GRACE_PERIOD_SECONDS);
    std::this_thread::sleep_for(std::chrono::seconds(STARTUP_GRACE_PERIOD_SECONDS));
    spdlog::info("GameState export: Performing startup full export with all captured data");
    export_gamestate();
  }

  auto last_export_time = std::chrono::steady_clock::now();

  while (!should_stop) {
    // Wait for either immediate export request, interval timeout, or CSV poll (10s)
    std::unique_lock<std::mutex> lock(export_request_mutex);

    if (cfg.export_gamestate_interval > 0) {
      export_cv.wait_for(lock, std::chrono::seconds(std::min(cfg.export_gamestate_interval, 10)), 
                         [] { return should_export_now || should_stop; });
    } else {
      export_cv.wait_for(lock, std::chrono::seconds(10),
                         [] { return should_export_now || should_stop; });
    }

    if (should_stop) {
      break;
    }

    // Decide whether to export full or differential
    bool should_export_full = false;
    bool force_full = should_force_full_export;
    should_force_full_export = false;
    auto now = std::chrono::steady_clock::now();
    auto seconds_since_last_full = std::chrono::duration_cast<std::chrono::seconds>(now - last_full_export_tp).count();

    // Force full export if explicitly requested, every hour, or if no baseline exists
    if (force_full || last_full_export_time.empty() || seconds_since_last_full >= 3600) {
      should_export_full = true;
    } else {
      // Calculate change percentage
      double change_pct = calculate_change_percentage();

      // Export full if more than 10% of data changed
      if (change_pct >= 0.10) {
        spdlog::debug("GameState export: {:.1f}% of data changed, triggering full export", change_pct * 100.0);
        should_export_full = true;
      }
    }

    // Export if explicitly requested or interval has elapsed
    auto seconds_since_last_export = std::chrono::duration_cast<std::chrono::seconds>(now - last_export_time).count();
    bool interval_elapsed = cfg.export_gamestate_interval > 0 && seconds_since_last_export >= cfg.export_gamestate_interval;

    if (should_export_now || interval_elapsed) {
      should_export_now = false;
      lock.unlock();

      if (should_export_full) {
        export_gamestate();
      } else {
        export_differential();
      }

      last_export_time = now;
    } else {
      lock.unlock();
    }

    // Poll prime_Data/ for new battle CSV files
    if (std::filesystem::exists(csv_dir)) {
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
      const auto& alert_cfg = Config::Get().resource_alerts;
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

  if (!cfg.export_gamestate) {
    spdlog::info("GameState export is disabled in config");
    return;
  }

  spdlog::info("GameState export: Enabled with interval={} seconds, path='{}', on_startup={}",
               cfg.export_gamestate_interval,
               cfg.export_gamestate_path.empty() ? "<game directory>" : cfg.export_gamestate_path,
               cfg.export_gamestate_on_startup);

  if (cfg.export_gamestate_gist.enabled) {
    spdlog::info("GameState Gist sync: Enabled for gist_id={}", cfg.export_gamestate_gist.gist_id);
    spdlog::info("GameState Gist sync: Full URL: https://gist.githubusercontent.com/raw/{}/{}",
                 cfg.export_gamestate_gist.gist_id, cfg.export_gamestate_gist.filename_full);
    spdlog::info("GameState Gist sync: Delta URL: https://gist.githubusercontent.com/raw/{}/{}",
                 cfg.export_gamestate_gist.gist_id, cfg.export_gamestate_gist.filename_delta);
  } else {
    spdlog::info("GameState Gist sync: Disabled (set [gamestate_export.gist] enabled=true to activate)");
  }

  // Record startup time for grace period
  startup_time = std::chrono::steady_clock::now();

  // Load ID mappings — anchor to the exe directory so this works regardless
  // of what the process working directory happens to be.
  std::filesystem::path mappings_path = File::ExeDir() / "game_data_maps" / "stfc_id_mappings.json";
  if (std::filesystem::exists(mappings_path)) {
    id_mappings::MappingCache::Get().load_mappings(mappings_path.string());
  } else {
    spdlog::warn("GameState export: ID mappings file not found at {}. Exported JSON will only contain IDs.", 
                 mappings_path.string());
  }

  should_stop = false;
  export_thread = std::thread(export_thread_func);
}

void export_now()
{
  export_gamestate();
}

} // namespace gamestate_export
