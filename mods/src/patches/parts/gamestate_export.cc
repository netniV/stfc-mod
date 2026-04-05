#include "gamestate_export.h"
#include "id_mappings.h"
#include "../../config.h"
#include "../../file.h"
#include "../../version.h"

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

using json = nlohmann::json;

namespace gamestate_export
{

static std::thread export_thread;
static bool        should_stop = false;
static bool        should_export_now = false;
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

static void request_immediate_export()
{
  std::scoped_lock lock(export_request_mutex);
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
  cached_player_data = data;
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
  for (const auto& module : data) {
    const auto id = module["id"].get<int64_t>();
    const auto level = module["level"].get<int32_t>();
    cached_buildings[id] = level;
  }
  request_immediate_export();
}

void capture_ships(const nlohmann::json& data)
{
  std::scoped_lock lock(game_data_mutex);
  for (const auto& ship : data) {
    const auto id = ship["id"].get<int64_t>();
    cached_ships[id] = {
      {"hull_id", ship["hull_id"]},
      {"tier", ship["tier"]},
      {"level", ship["level"]},
      {"level_percentage", ship["level_percentage"]},
      {"components", ship.value("components", json::array())}
    };
  }
  request_immediate_export();
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

  // Officers - enrich with names
  j["officers"] = json::array();
  for (const auto& [id, officer_data] : cached_officers) {
    json officer_entry = officer_data;
    officer_entry["id"] = id;
    id_mappings::MappingCache::Get().enrich_officer(officer_entry);
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

  // Placeholders for future implementation
  j["faction_reputation"] = json::array();
  j["blueprints"] = json::array();

  spdlog::debug("GameState JSON structure built: {} buildings, {} research, {} ships, {} officers, {} resources",
                cached_buildings.size(), cached_research.size(), cached_ships.size(), 
                cached_officers.size(), cached_resources.size());

  return j;
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

  } catch (const std::exception& e) {
    spdlog::error("GameState differential export: Exception during export: {}", e.what());
  }
}

void export_thread_func()
{
  auto& cfg = Config::Get();

  spdlog::info("GameState export thread started");

  // Always export full snapshot on startup
  if (cfg.export_gamestate_on_startup) {
    spdlog::info("GameState export: Performing startup full export");
    export_gamestate();
  }

  auto last_export_time = std::chrono::steady_clock::now();

  while (!should_stop) {
    // Wait for either immediate export request or interval timeout
    std::unique_lock<std::mutex> lock(export_request_mutex);

    if (cfg.export_gamestate_interval > 0) {
      export_cv.wait_for(lock, std::chrono::seconds(cfg.export_gamestate_interval), 
                         [] { return should_export_now || should_stop; });
    } else {
      export_cv.wait(lock, [] { return should_export_now || should_stop; });
    }

    if (should_stop) {
      break;
    }

    // Decide whether to export full or differential
    bool should_export_full = false;
    auto now = std::chrono::steady_clock::now();
    auto seconds_since_last_full = std::chrono::duration_cast<std::chrono::seconds>(now - last_full_export_tp).count();

    // Force full export every hour (3600 seconds) or if no baseline exists
    if (last_full_export_time.empty() || seconds_since_last_full >= 3600) {
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

    // Export if requested or interval elapsed
    if (should_export_now || cfg.export_gamestate_interval > 0) {
      should_export_now = false;
      lock.unlock();

      if (should_export_full) {
        export_gamestate();
      } else {
        export_differential();
      }

      last_export_time = now;
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

  // Load ID mappings
  std::filesystem::path mappings_path = "game_data_maps/stfc_id_mappings.json";
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
