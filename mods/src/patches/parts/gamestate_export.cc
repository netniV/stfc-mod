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

// Cached game data
static std::mutex game_data_mutex;
static json cached_player_data;
static std::unordered_map<int64_t, int64_t> cached_resources;
static std::unordered_map<int64_t, int32_t> cached_buildings;
static std::unordered_map<int64_t, json> cached_ships;
static std::unordered_map<int64_t, int32_t> cached_research;
static std::unordered_map<uint64_t, json> cached_officers;

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
      export_path = "gamestate_export.json";
    } else {
      std::filesystem::path export_dir(cfg.export_gamestate_path);
      if (!std::filesystem::exists(export_dir)) {
        std::error_code ec;
        std::filesystem::create_directories(export_dir, ec);
        if (ec) {
          spdlog::error("GameState export: Failed to create directory {}: {}", cfg.export_gamestate_path, ec.message());
          export_path = "gamestate_export.json";
        } else {
          export_path = (export_dir / "gamestate_export.json").string();
        }
      } else {
        export_path = (export_dir / "gamestate_export.json").string();
      }
    }

    json gamestate = build_gamestate_json();

    std::ofstream out(export_path);
    if (!out.is_open()) {
      spdlog::error("GameState export: Failed to open file for writing: {}", export_path);
      return;
    }

    out << gamestate.dump(2);
    out.close();

    spdlog::info("GameState export: Successfully exported to {}", export_path);

  } catch (const std::exception& e) {
    spdlog::error("GameState export: Exception during export: {}", e.what());
  }
}

void export_thread_func()
{
  auto& cfg = Config::Get();

  spdlog::info("GameState export thread started");

  if (cfg.export_gamestate_on_startup) {
    spdlog::info("GameState export: Performing startup export");
    export_gamestate();
  }

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

    // Export if requested or interval elapsed
    if (should_export_now || cfg.export_gamestate_interval > 0) {
      should_export_now = false;
      lock.unlock();
      export_gamestate();
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
