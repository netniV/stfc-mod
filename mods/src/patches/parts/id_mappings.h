#pragma once

#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <optional>
#include <vector>

namespace id_mappings
{

struct ItemMapping
{
  std::string name;
  std::string category;  // For research, buildings, etc.
  std::string faction;   // For officers
  std::string rarity;    // For officers, resources
  std::vector<uint64_t> trait_ids;  // For officers
};

class MappingCache
{
public:
  static MappingCache& Get();

  // Initialize from mappings file
  void load_mappings(const std::string& file_path);

  // Check if mappings are loaded
  bool is_loaded() const { return loaded_; }

  // Lookup functions
  std::optional<ItemMapping> get_officer(uint64_t id) const;
  std::optional<ItemMapping> get_research(int64_t id) const;
  std::optional<ItemMapping> get_building(int64_t id) const;
  std::optional<ItemMapping> get_resource(int64_t id) const;
  std::optional<ItemMapping> get_ship(int64_t id) const;
  std::optional<ItemMapping> get_trait(uint64_t id) const;

  // Helper to enrich JSON with names
  void enrich_officer(nlohmann::json& officer_json) const;
  void enrich_research(nlohmann::json& research_json) const;
  void enrich_building(nlohmann::json& building_json) const;
  void enrich_resource(nlohmann::json& resource_json, int64_t id) const;
  void enrich_ship(nlohmann::json& ship_json) const;

private:
  MappingCache() = default;

  bool loaded_ = false;
  std::unordered_map<uint64_t, ItemMapping> officer_mappings_;
  std::unordered_map<int64_t, ItemMapping> research_mappings_;
  std::unordered_map<int64_t, ItemMapping> building_mappings_;
  std::unordered_map<int64_t, ItemMapping> resource_mappings_;
  std::unordered_map<int64_t, ItemMapping> ship_mappings_;
  std::unordered_map<uint64_t, ItemMapping> trait_mappings_;
};

} // namespace id_mappings
