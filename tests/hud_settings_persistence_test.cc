#include "runtime_config_writer.h"
#include "patches/parts/runtime_config_keys.h"
#include <toml++/toml.h>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <thread>

namespace
{
void Check(bool value)
{
  if (!value) throw std::runtime_error("HUD persistence fixture failed");
}
} // namespace

int main(int argc, char** argv)
{
  Check(argc == 2);
  const std::filesystem::path directory(argv[1]);
  std::filesystem::create_directories(directory);
  const auto path = directory / "hud.toml";
  {
    std::ofstream output(path);
    output << "# keep this comment\n[ui]\nunrelated = true\n";
    for (const char* key : {"hud_q_trials", "hud_field_training", "hud_outposts", "hud_missions"})
      output << key << " = 'auto'\n";
  }
  const auto initial = toml::parse_file(path.string());
  config_edit::RuntimeConfigWriter writer(path, std::nullopt);
  for (const auto& [section, key] : config_edit::persisted_settings) {
    const auto value = initial[section][key].value<std::string>();
    Check(writer.Register(section, key, value ? std::optional<config_edit::Value>(*value) : std::nullopt));
  }
  for (const char* mode : {"always", "never", "auto"}) {
    for (const char* key : {"hud_q_trials", "hud_field_training", "hud_outposts", "hud_missions"}) {
      const auto revision = writer.Submit("ui", key, std::string(mode));
      Check(revision != 0);
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
      while (writer.LastCompletion().revision != revision || writer.HasWork()) {
        Check(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
      }
      Check(!writer.HasFailures());
      const auto loaded = toml::parse_file(path.string());
      Check(loaded["ui"][key].value<std::string>() == mode);
      Check(loaded["ui"]["unrelated"].value<bool>() == true);
    }
  }
  writer.Stop(false);
  while (!writer.PollStopped()) std::this_thread::yield();
  std::ifstream input(path);
  std::string first_line;
  std::getline(input, first_line);
  Check(first_line == "# keep this comment");
}
