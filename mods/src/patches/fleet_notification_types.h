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
};

inline constexpr std::array kFleetNotificationCatalog{
    FleetNotificationCatalogEntry{FleetNotificationKind::ArrivedInSystem, "ArrivedInSystem"},
    FleetNotificationCatalogEntry{FleetNotificationKind::ArrivedAtDestination, "ArrivedAtDestination"},
    FleetNotificationCatalogEntry{FleetNotificationKind::StartedMining, "StartedMining"},
    FleetNotificationCatalogEntry{FleetNotificationKind::NodeDepleted, "NodeDepleted"},
    FleetNotificationCatalogEntry{FleetNotificationKind::Docked, "Docked"},
    FleetNotificationCatalogEntry{FleetNotificationKind::RepairComplete, "RepairComplete"},
    FleetNotificationCatalogEntry{FleetNotificationKind::MinerOpc, "MinerOPC"},
};

static_assert(kFleetNotificationCatalog.size() == static_cast<std::size_t>(FleetNotificationKind::Count));

constexpr FleetNotificationMask fleet_notification_bit(FleetNotificationKind kind)
{ return FleetNotificationMask{1} << static_cast<uint8_t>(kind); }

constexpr FleetNotificationMask kAllFleetNotifications = [] {
  FleetNotificationMask result = 0;
  for (const auto& entry : kFleetNotificationCatalog) {
    result |= fleet_notification_bit(entry.kind);
  }
  return result;
}();
