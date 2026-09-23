#include "id_mappings.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <filesystem>

using json = nlohmann::json;

namespace id_mappings
{

MappingCache& MappingCache::Get()
{
  static MappingCache instance;
  return instance;
}

void MappingCache::load_mappings(const std::string& file_path)
{
  try {
    if (!std::filesystem::exists(file_path)) {
      spdlog::warn("ID mappings file not found: {}. Names will not be enriched.", file_path);
      return;
    }

    std::ifstream file(file_path);
    if (!file.is_open()) {
      spdlog::error("Failed to open ID mappings file: {}", file_path);
      return;
    }

    json mappings_data;
    file >> mappings_data;
    file.close();

    // Load officers
    if (mappings_data.contains("officers")) {
      for (const auto& [id_str, data] : mappings_data["officers"].items()) {
        uint64_t id = std::stoull(id_str);
        ItemMapping mapping;
        mapping.name = data.value("name", "");
        mapping.faction = data.value("faction", "");
        mapping.rarity = data.value("rarity", "");
        mapping.category = data.value("synergy", "");

        // Load trait IDs if present
        if (data.contains("trait_ids") && data["trait_ids"].is_array()) {
          for (const auto& trait_id : data["trait_ids"]) {
            if (trait_id.is_number_unsigned()) {
              mapping.trait_ids.push_back(trait_id.get<uint64_t>());
            }
          }
        }

        officer_mappings_[id] = mapping;
      }
      spdlog::info("Loaded {} officer name mappings", officer_mappings_.size());
    }

    // Load research
    if (mappings_data.contains("research")) {
      for (const auto& [id_str, data] : mappings_data["research"].items()) {
        int64_t id = std::stoll(id_str);
        ItemMapping mapping;
        mapping.name = data.value("name", "");
        mapping.category = data.value("category", "");
        research_mappings_[id] = mapping;
      }
      spdlog::info("Loaded {} research name mappings", research_mappings_.size());
    }

    // Load buildings
    if (mappings_data.contains("buildings")) {
      for (const auto& [id_str, data] : mappings_data["buildings"].items()) {
        int64_t id = std::stoll(id_str);
        ItemMapping mapping;
        mapping.name = data.value("name", "");
        mapping.category = data.value("key", "");
        building_mappings_[id] = mapping;
      }
      spdlog::info("Loaded {} building name mappings", building_mappings_.size());
    }

    // Load resources
    if (mappings_data.contains("resources")) {
      for (const auto& [id_str, data] : mappings_data["resources"].items()) {
        int64_t id = std::stoll(id_str);
        ItemMapping mapping;
        mapping.name = data.value("name", "");
        mapping.category = data.value("group", "");
        mapping.rarity = std::to_string(data.value("rarity", 0));
        resource_mappings_[id] = mapping;
      }
      spdlog::info("Loaded {} resource name mappings", resource_mappings_.size());
    }

    // Load ships (if available)
    if (mappings_data.contains("ships")) {
      for (const auto& [id_str, data] : mappings_data["ships"].items()) {
        int64_t id = std::stoll(id_str);
        ItemMapping mapping;
        mapping.name = data.value("name", "");
        mapping.category = data.value("class", "");
        ship_mappings_[id] = mapping;
      }
      spdlog::info("Loaded {} ship name mappings", ship_mappings_.size());
    }

    // Load traits (if available)
    if (mappings_data.contains("traits")) {
      for (const auto& [id_str, data] : mappings_data["traits"].items()) {
        uint64_t id = std::stoull(id_str);
        ItemMapping mapping;
        mapping.name = data.value("name", "");
        mapping.category = data.value("description", "");
        trait_mappings_[id] = mapping;
      }
      spdlog::info("Loaded {} trait name mappings", trait_mappings_.size());
    }

    loaded_ = true;
    spdlog::info("Successfully loaded ID mappings from {}", file_path);

  } catch (const std::exception& e) {
    spdlog::error("Failed to load ID mappings: {}", e.what());
    loaded_ = false;
  }
}

std::optional<ItemMapping> MappingCache::get_officer(uint64_t id) const
{
  auto it = officer_mappings_.find(id);
  if (it != officer_mappings_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<ItemMapping> MappingCache::get_research(int64_t id) const
{
  auto it = research_mappings_.find(id);
  if (it != research_mappings_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<ItemMapping> MappingCache::get_building(int64_t id) const
{
  auto it = building_mappings_.find(id);
  if (it != building_mappings_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<ItemMapping> MappingCache::get_resource(int64_t id) const
{
  auto it = resource_mappings_.find(id);
  if (it != resource_mappings_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<ItemMapping> MappingCache::get_ship(int64_t id) const
{
  auto it = ship_mappings_.find(id);
  if (it != ship_mappings_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<ItemMapping> MappingCache::get_trait(uint64_t id) const
{
  auto it = trait_mappings_.find(id);
  if (it != trait_mappings_.end()) {
    return it->second;
  }
  return std::nullopt;
}

void MappingCache::enrich_officer(json& officer_json) const
{
  if (!officer_json.contains("id")) return;

  uint64_t id = officer_json["id"].get<uint64_t>();
  auto mapping = get_officer(id);

  if (mapping) {
    officer_json["name"] = mapping->name;
    if (!mapping->faction.empty()) {
      officer_json["faction"] = mapping->faction;
    }
    if (!mapping->rarity.empty()) {
      officer_json["rarity"] = mapping->rarity;
    }
    if (!mapping->category.empty()) {
      officer_json["synergy"] = mapping->category;
    }

    // Add traits if available
    if (!mapping->trait_ids.empty()) {
      json traits_array = json::array();

      for (uint64_t trait_id : mapping->trait_ids) {
        json trait_obj;
        trait_obj["id"] = trait_id;

        // Look up trait name
        auto trait_mapping = get_trait(trait_id);
        if (trait_mapping) {
          trait_obj["name"] = trait_mapping->name;
          if (!trait_mapping->category.empty()) {
            trait_obj["description"] = trait_mapping->category;
          }
        } else {
          trait_obj["name"] = "Unknown Trait";
        }

        traits_array.push_back(trait_obj);
      }

      officer_json["traits"] = traits_array;
    }
  }
}

void MappingCache::enrich_research(json& research_json) const
{
  if (!research_json.contains("id")) return;

  int64_t id = research_json["id"].get<int64_t>();
  auto mapping = get_research(id);

  if (mapping) {
    research_json["name"] = mapping->name;
    if (!mapping->category.empty()) {
      research_json["tree"] = mapping->category;
    }
  }
}

void MappingCache::enrich_building(json& building_json) const
{
  if (!building_json.contains("id")) return;

  int64_t id = building_json["id"].get<int64_t>();
  auto mapping = get_building(id);

  if (mapping) {
    building_json["name"] = mapping->name;
    if (!mapping->category.empty()) {
      building_json["key"] = mapping->category;
    }
  }
}

void MappingCache::enrich_resource(json& resource_json, int64_t id) const
{
  auto mapping = get_resource(id);

  if (mapping) {
    resource_json["name"] = mapping->name;
    if (!mapping->category.empty()) {
      resource_json["group"] = mapping->category;
    }
    if (!mapping->rarity.empty()) {
      resource_json["rarity"] = mapping->rarity;
    }
  }
}

void MappingCache::enrich_ship(json& ship_json) const
{
  if (!ship_json.contains("hull_id")) return;

  int64_t hull_id = ship_json["hull_id"].get<int64_t>();
  auto mapping = get_ship(hull_id);

  if (mapping) {
    ship_json["name"] = mapping->name;
    if (!mapping->category.empty()) {
      ship_json["class"] = mapping->category;
    }
  }
}

} // namespace id_mappings
