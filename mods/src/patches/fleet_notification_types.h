#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

enum class FleetNotificationKind : uint8_t {
  ArrivedInSystem = 0,
  ArrivedAtDestination,
  StartedMining,
  NodeDepleted,
  Docked,
  RepairComplete,
  MinerOpc,
  Count,
};

using FleetNotificationMask = uint32_t;

struct FleetNotificationCatalogEntry {
  FleetNotificationKind kind;
  std::string_view      config_name;
  std::string_view      audio_config_name;
};

inline constexpr std::array kFleetNotificationCatalog{
    FleetNotificationCatalogEntry{FleetNotificationKind::ArrivedInSystem, "ArrivedInSystem",
                                  "alert_fleet_arrived_in_system"},
    FleetNotificationCatalogEntry{FleetNotificationKind::ArrivedAtDestination, "ArrivedAtDestination",
                                  "alert_fleet_arrived_at_destination"},
    FleetNotificationCatalogEntry{FleetNotificationKind::StartedMining, "StartedMining",
                                  "alert_fleet_started_mining"},
    FleetNotificationCatalogEntry{FleetNotificationKind::NodeDepleted, "NodeDepleted",
                                  "alert_fleet_node_depleted"},
    FleetNotificationCatalogEntry{FleetNotificationKind::Docked, "Docked", "alert_fleet_docked"},
    FleetNotificationCatalogEntry{FleetNotificationKind::RepairComplete, "RepairComplete",
                                  "alert_fleet_repair_complete"},
    FleetNotificationCatalogEntry{FleetNotificationKind::MinerOpc, "MinerOPC", "alert_fleet_miner_opc"},
};

static_assert(kFleetNotificationCatalog.size() == static_cast<std::size_t>(FleetNotificationKind::Count));

constexpr FleetNotificationMask fleet_notification_bit(FleetNotificationKind kind)
{ return FleetNotificationMask{1} << static_cast<uint8_t>(kind); }

constexpr std::size_t fleet_notification_index(FleetNotificationKind kind)
{ return static_cast<std::size_t>(kind); }

constexpr FleetNotificationMask kAllFleetNotifications = [] {
  FleetNotificationMask result = 0;
  for (const auto& entry : kFleetNotificationCatalog) {
    result |= fleet_notification_bit(entry.kind);
  }
  return result;
}();
