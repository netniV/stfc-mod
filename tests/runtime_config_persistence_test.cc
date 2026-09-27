#include "runtime_config_writer.h"
#include "config_save.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>

int main(int argc, char** argv)
{
  assert(argc == 2);
  const auto path = std::filesystem::path(argv[1]) / "community_patch_settings.toml";
  std::filesystem::create_directories(path.parent_path());
  const std::string original = "# keep this comment\n[graphics]\ngalaxy_multi_select = false\n"
      "galaxy_label_major_detail = \"native\"\ngalaxy_label_major_threshold = 0.5\n"
      "[unrelated]\nvalue = 42\n";
  ReplaceConfigText(path, original);
  {
    config_edit::RuntimeConfigWriter writer(path, std::nullopt);
    assert(writer.Register("graphics", "galaxy_multi_select", false));
    assert(writer.Register("graphics", "galaxy_label_major_detail", std::string("native")));
    assert(writer.Register("graphics", "galaxy_label_major_threshold", 0.5));
    assert(writer.Submit("graphics", "galaxy_multi_select", true));
    assert(writer.Submit("graphics", "galaxy_label_major_detail", std::string("threshold")));
    // Quit immediately with a slider write still debounced: orderly stop must flush it.
    assert(writer.Submit("graphics", "galaxy_label_major_threshold", 0.95, std::chrono::seconds(30)));
    writer.Stop(false);
  }
  // Fresh parse models a new process, with no access to live Config or writer state.
  const auto bytes = ReadConfigText(path);
  const auto loaded = toml::parse(bytes);
  assert(loaded["graphics"]["galaxy_multi_select"].value<bool>() == true);
  assert(loaded["graphics"]["galaxy_label_major_detail"].value<std::string>() == "threshold");
  assert(loaded["graphics"]["galaxy_label_major_threshold"].value<double>() == 0.95);
  assert(loaded["unrelated"]["value"].value<int>() == 42);
  assert(bytes.starts_with("# keep this comment"));
  std::cout << "Runtime settings disk/reload regression passed\n";
}
