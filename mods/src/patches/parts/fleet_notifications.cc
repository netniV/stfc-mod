#include "patches/fleet_notification_types.h"

#include "config.h"
#include "errormsg.h"
#include "patches/fleet_watch.h"
#include "patches/notification_service.h"
#include "str_utils.h"

#include <il2cpp/il2cpp_helper.h>
#include <prime/FleetsManager.h>

#include <spud/detour.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <spdlog/spdlog.h>

namespace
{
constexpr int kFleetSlotCount = 10;

using TransitionPredicate = bool (*)(FleetState before, FleetState after);

struct TransitionRule {
  FleetNotificationKind kind;
  TransitionPredicate   matches;
  std::string_view      title;
  std::string_view      message;
};

constexpr bool state_is_destination(FleetState state)
{
  switch (state) {
    case FleetState::IdleInSpace:
    case FleetState::CanRecall:
    case FleetState::CanEngage:
    case FleetState::CanLocate:
    case FleetState::Deployed:
    case FleetState::Capturing:
      return true;
    default:
      return false;
  }
}

constexpr bool state_can_dock_from_space(FleetState state)
{
  switch (state) {
    case FleetState::IdleInSpace:
    case FleetState::Mining:
    case FleetState::Battling:
    case FleetState::WarpCharging:
    case FleetState::Warping:
    case FleetState::Impulsing:
    case FleetState::Capturing:
    case FleetState::CanRecall:
    case FleetState::CanEngage:
    case FleetState::CanLocate:
    case FleetState::Deployed:
      return true;
    default:
      return false;
  }
}

constexpr bool arrived_in_system(FleetState before, FleetState after)
{ return before == FleetState::Warping && after == FleetState::Impulsing; }

constexpr bool arrived_at_destination(FleetState before, FleetState after)
{ return before == FleetState::Impulsing && state_is_destination(after); }

constexpr bool started_mining(FleetState before, FleetState after)
{ return before != FleetState::Mining && after == FleetState::Mining; }

constexpr bool docked(FleetState before, FleetState after)
{ return before != FleetState::Repairing && after == FleetState::Docked && state_can_dock_from_space(before); }

constexpr bool repair_complete(FleetState before, FleetState after)
{ return before == FleetState::Repairing && after == FleetState::Docked; }

constexpr std::array kTransitionRules{
    TransitionRule{FleetNotificationKind::ArrivedInSystem, arrived_in_system, "Fleet Arrived", "has arrived in-system"},
    TransitionRule{FleetNotificationKind::ArrivedAtDestination, arrived_at_destination, "Fleet Arrived",
                   "has reached its destination"},
    TransitionRule{FleetNotificationKind::StartedMining, started_mining, "Fleet Mining", "started mining"},
    TransitionRule{FleetNotificationKind::Docked, docked, "Fleet Docked", "docked"},
    TransitionRule{FleetNotificationKind::RepairComplete, repair_complete, "Repair Complete", "finished repairs"},
};

static_assert(arrived_in_system(FleetState::Warping, FleetState::Impulsing));
static_assert(arrived_at_destination(FleetState::Impulsing, FleetState::IdleInSpace));
static_assert(started_mining(FleetState::IdleInSpace, FleetState::Mining));
static_assert(docked(FleetState::Impulsing, FleetState::Docked));
static_assert(!docked(FleetState::Repairing, FleetState::Docked));
static_assert(repair_complete(FleetState::Repairing, FleetState::Docked));

FleetNotificationMask s_enabled_notifications = 0;

bool notification_enabled(FleetNotificationKind kind)
{ return (s_enabled_notifications & fleet_notification_bit(kind)) != 0; }

std::string normalize_name(std::string name)
{
  constexpr std::string_view live_suffix = "_LIVE";
  if (name.ends_with(live_suffix)) {
    name.erase(name.size() - live_suffix.size());
  }
  for (auto& character : name) {
    if (character == '_') {
      character = ' ';
    }
  }
  return name;
}

std::string fleet_subject(FleetPlayerData* fleet)
{
  auto* hull = fleet ? fleet->Hull : nullptr;
  return hull && hull->Name ? normalize_name(to_string(hull->Name)) : "fleet";
}

void emit_transition(const fleet_watch::Transition& transition)
{
  for (const auto& rule : kTransitionRules) {
    if (!notification_enabled(rule.kind) || !rule.matches(transition.before.state, transition.after.state)) {
      continue;
    }
#ifdef _MODDBG
    spdlog::info("[FleetNotificationsProbe] event={} fleet={} oldState={} newState={}",
                 fleet_notification_name(rule.kind), transition.after.fleet_id,
                 static_cast<int>(transition.before.state), static_cast<int>(transition.after.state));
#endif
    notification_emit(rule.title, "Your " + fleet_subject(transition.fleet) + " " + std::string{rule.message});
  }
}

constexpr bool needs_fast_poll(FleetNotificationMask enabled, FleetState state)
{
  constexpr auto arrival_events       = fleet_notification_bit(FleetNotificationKind::ArrivedInSystem)
                                        | fleet_notification_bit(FleetNotificationKind::ArrivedAtDestination);
  constexpr auto post_warp_events     = arrival_events | fleet_notification_bit(FleetNotificationKind::StartedMining)
                                        | fleet_notification_bit(FleetNotificationKind::Docked);
  constexpr auto post_impulse_events  = fleet_notification_bit(FleetNotificationKind::ArrivedAtDestination)
                                        | fleet_notification_bit(FleetNotificationKind::StartedMining)
                                        | fleet_notification_bit(FleetNotificationKind::Docked);
  constexpr auto post_activity_events = fleet_notification_bit(FleetNotificationKind::StartedMining)
                                        | fleet_notification_bit(FleetNotificationKind::Docked);

  switch (state) {
    case FleetState::WarpCharging:
    case FleetState::Warping:
      return (enabled & post_warp_events) != 0;
    case FleetState::Impulsing:
      return (enabled & post_impulse_events) != 0;
    case FleetState::Battling:
    case FleetState::Capturing:
      return (enabled & post_activity_events) != 0;
    case FleetState::Repairing:
      return (enabled & fleet_notification_bit(FleetNotificationKind::RepairComplete)) != 0;
    default:
      return false;
  }
}

bool needs_enabled_fast_poll(FleetState state)
{ return needs_fast_poll(s_enabled_notifications, state); }

static_assert(needs_fast_poll(fleet_notification_bit(FleetNotificationKind::ArrivedInSystem), FleetState::Warping));
static_assert(!needs_fast_poll(fleet_notification_bit(FleetNotificationKind::ArrivedInSystem), FleetState::Impulsing));
static_assert(needs_fast_poll(fleet_notification_bit(FleetNotificationKind::StartedMining), FleetState::Battling));
static_assert(needs_fast_poll(fleet_notification_bit(FleetNotificationKind::RepairComplete), FleetState::Repairing));

struct RecentNodeDepletion {
  uint64_t                              fleet_id = 0;
  std::chrono::steady_clock::time_point emitted_at{};
};

std::array<RecentNodeDepletion, kFleetSlotCount> s_recent_node_depletions{};
std::size_t                                      s_next_node_depletion = 0;

bool allow_node_depletion(uint64_t fleet_id)
{
  constexpr auto duplicate_window = std::chrono::seconds{2};
  const auto     now              = std::chrono::steady_clock::now();
  for (const auto& recent : s_recent_node_depletions) {
    if (recent.emitted_at != std::chrono::steady_clock::time_point{} && recent.fleet_id == fleet_id
        && now - recent.emitted_at < duplicate_window) {
      return false;
    }
  }
  s_recent_node_depletions[s_next_node_depletion] = {fleet_id, now};
  s_next_node_depletion                           = (s_next_node_depletion + 1) % s_recent_node_depletions.size();
  return true;
}

FleetPlayerData* find_fleet(uint64_t fleet_id)
{
  auto* manager = FleetsManager::Instance();
  if (!manager || !manager->HasFleetService()) {
    return nullptr;
  }
  for (int slot = 0; slot < kFleetSlotCount; ++slot) {
    auto* fleet = manager->GetFleetPlayerData(slot);
    if (fleet && fleet->Id == fleet_id) {
      return fleet;
    }
  }
  return nullptr;
}

void ToastFleetObserver_HandleMiningDepleted_Hook(auto original, void* self, int64_t fleet_id)
{
  {
    ScopedToastNotificationSuppression suppress_duplicate_forwarding;
    original(self, fleet_id);
  }
  const auto id = static_cast<uint64_t>(fleet_id);
  if (!allow_node_depletion(id)) {
    return;
  }
#ifdef _MODDBG
  spdlog::info("[FleetNotificationsProbe] event={} fleet={}",
               fleet_notification_name(FleetNotificationKind::NodeDepleted), id);
#endif
  auto* fleet = find_fleet(id);
  notification_emit("Node Depleted", fleet ? "Your " + fleet_subject(fleet) + " depleted its node."
                                           : "A mining fleet depleted its node.");
}

bool install_node_depletion_hook()
{
  auto helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "ToastFleetObserver");
  if (!helper.isValidHelper()) {
    ErrorMsg::MissingHelper("HUD", "ToastFleetObserver");
    return false;
  }
  auto* method = helper.GetMethod("HandleMiningDepleted", 1);
  if (!method) {
    ErrorMsg::MissingMethod("ToastFleetObserver", "HandleMiningDepleted");
    return false;
  }
  SPUD_STATIC_DETOUR(method, ToastFleetObserver_HandleMiningDepleted_Hook);
  return true;
}
} // namespace

void InstallFleetNotificationHooks()
{
#if _WIN32
  s_enabled_notifications = Config::Get().notify_fleet_events;
  notification_init();

  constexpr auto transition_notifications =
      kAllFleetNotifications & ~fleet_notification_bit(FleetNotificationKind::NodeDepleted);
  if ((s_enabled_notifications & transition_notifications) != 0
      && !fleet_watch::Subscribe({emit_transition, needs_enabled_fast_poll})) {
    spdlog::warn("[FleetNotifications] Fleet Watch subscription failed");
  }
  if (notification_enabled(FleetNotificationKind::NodeDepleted) && !install_node_depletion_hook()) {
    spdlog::warn("[FleetNotifications] node-depletion hook installation failed");
  }
#endif
}
