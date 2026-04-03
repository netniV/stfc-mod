#include "gamestate_export.h"
#include "../../config.h"
#include "../../file.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>

using json = nlohmann::json;

namespace gamestate_export
{

static std::thread export_thread;
static bool        should_stop = false;

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

json build_gamestate_json()
{
  json j;

  j["meta"]["version"] = "1.0.0";
  j["meta"]["exported_at"] = get_iso8601_timestamp();
  j["meta"]["mod_version"] = "0.6.0"; // TODO: Pull from version.h

  // Player info placeholder
  j["player"]["ops_level"] = 0;
  j["player"]["name"] = "";
  j["player"]["alliance"] = "";
  j["player"]["power"] = 0;

  // Buildings placeholder
  j["buildings"] = json::array();

  // Research placeholder
  j["research"] = json::array();

  // Ships placeholder
  j["ships"] = json::array();

  // Faction reputation placeholder
  j["faction_reputation"] = json::array();

  // Resources placeholder
  j["resources"] = json::object();

  // Blueprints placeholder
  j["blueprints"] = json::array();

  spdlog::debug("GameState JSON structure built (placeholder data)");

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
    if (cfg.export_gamestate_interval > 0) {
      std::this_thread::sleep_for(std::chrono::seconds(cfg.export_gamestate_interval));

      if (!should_stop) {
        export_gamestate();
      }
    } else {
      std::this_thread::sleep_for(std::chrono::seconds(1));
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

  should_stop = false;
  export_thread = std::thread(export_thread_func);
}

void export_now()
{
  export_gamestate();
}

} // namespace gamestate_export
