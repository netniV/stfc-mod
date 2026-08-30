#include "patches/fleet_watch.h"

#include "config.h"
#include "errormsg.h"
#include "patches/fleet_notification_types.h"
#include "patches/notification_service.h"
#include "str_utils.h"

#include <prime/FleetPlayerData.h>
#include <prime/FleetsManager.h>
#include <prime/SpecManager.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace
{
constexpr int     kFleetSlotCount            = 10;
constexpr int64_t kFollowThroughIntervalMs   = 250;
constexpr int64_t kFollowThroughBackoffMs    = 5'000;
constexpr int64_t kFollowThroughFastPeriodMs = 30'000;
constexpr int64_t kFollowThroughLifetimeMs   = 24LL * 60 * 60 * 1'000;
constexpr int64_t kMiningWatchIntervalMs     = 5'000;
constexpr int64_t kSeedFastIntervalMs        = 1'000;
constexpr int64_t kSeedBackoffIntervalMs     = 5'000;
constexpr int64_t kSeedFastPeriodMs          = 30'000;
constexpr int64_t kSeedLifetimeMs            = 120'000;
constexpr int64_t kSeedStabilizationMs       = 5'000;
constexpr int64_t kSeedMaxStabilizationMs    = 30'000;
constexpr int64_t kDuplicateSuppressionMs    = 2'000;

constexpr std::size_t kFleetNotificationCount = static_cast<std::size_t>(FleetNotificationKind::Count);

struct FleetSnapshot {
  uint64_t   fleet_id                  = 0;
  int64_t    resource_id               = 0;
  FleetState state                     = FleetState::Unknown;
  bool       occupied                  = false;
  bool       opc_known                 = false;
  bool       opc                       = false;
  int64_t    follow_through_started_ms = 0;
};

struct FleetNotificationStamp {
  uint64_t fleet_id        = 0;
  int64_t  last_emitted_ms = 0;
  bool     occupied        = false;
};

using TransitionPredicate = bool (*)(FleetState, FleetState);

struct FleetTransitionRule {
  FleetNotificationKind kind;
  TransitionPredicate   matches;
};

std::array<FleetSnapshot, kFleetSlotCount>                                s_slots{};
std::array<std::array<int64_t, kFleetNotificationCount>, kFleetSlotCount> s_last_emitted_ms{};
std::array<FleetNotificationStamp, kFleetSlotCount>                       s_node_depleted_stamps{};
FleetNotificationMask                                                     s_enabled_notifications       = 0;
bool                                                                      s_state_observation_enabled   = false;
bool                                                                      s_seed_pending                = false;
bool                                                                      s_seed_has_observation        = false;
bool                                                                      s_seed_ready_candidate        = false;
bool                                                                      s_seed_candidate_forced       = false;
bool                                                                      s_seed_restart_disabled       = false;
int64_t                                                                   s_last_follow_through_scan_ms = 0;
int64_t                                                                   s_last_mining_scan_ms         = 0;
int64_t                                                                   s_last_seed_attempt_ms        = 0;
int64_t                                                                   s_seed_started_ms             = 0;
int64_t                                                                   s_seed_first_observed_ms      = 0;
int64_t                                                                   s_seed_last_change_ms         = 0;
int                                                                       s_seed_candidate_count        = 0;
int                                                                       s_mining_watch_count          = 0;
int                                                                       s_follow_through_watch_count  = 0;

int64_t now_milliseconds();

void restart_seed_stabilization(int slot_index, uint64_t fleet_id)
{
  const auto now_ms        = now_milliseconds();
  s_seed_pending           = true;
  s_seed_has_observation   = false;
  s_seed_ready_candidate   = false;
  s_seed_candidate_forced  = false;
  s_last_seed_attempt_ms   = 0;
  s_seed_started_ms        = now_ms;
  s_seed_first_observed_ms = 0;
  s_seed_last_change_ms    = 0;
  s_seed_candidate_count   = 0;
  spdlog::debug("[FleetWatch] new fleet observed; restarting initial-state stabilization slot={} fleet={}", slot_index,
                fleet_id);
}

int64_t now_milliseconds()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

bool notification_enabled(FleetNotificationKind kind)
{ return (s_enabled_notifications & fleet_notification_bit(kind)) != 0; }

bool allow_notification(int slot_index, FleetNotificationKind kind)
{
  if (slot_index < 0 || slot_index >= kFleetSlotCount) {
    return true;
  }

  const auto now  = now_milliseconds();
  auto&      last = s_last_emitted_ms[slot_index][static_cast<std::size_t>(kind)];
  if (last != 0 && now - last < kDuplicateSuppressionMs) {
    return false;
  }
  last = now;
  return true;
}

constexpr bool state_requires_follow_through(FleetNotificationMask enabled, FleetState state)
{
  constexpr auto arrival_events       = fleet_notification_bit(FleetNotificationKind::ArrivedInSystem)
                                        | fleet_notification_bit(FleetNotificationKind::ArrivedAtDestination);
  constexpr auto post_warp_events     = arrival_events | fleet_notification_bit(FleetNotificationKind::StartedMining)
                                        | fleet_notification_bit(FleetNotificationKind::Docked)
                                        | fleet_notification_bit(FleetNotificationKind::MinerOpc);
  constexpr auto post_impulse_events  = fleet_notification_bit(FleetNotificationKind::ArrivedAtDestination)
                                        | fleet_notification_bit(FleetNotificationKind::StartedMining)
                                        | fleet_notification_bit(FleetNotificationKind::Docked)
                                        | fleet_notification_bit(FleetNotificationKind::MinerOpc);
  constexpr auto post_activity_events = fleet_notification_bit(FleetNotificationKind::StartedMining)
                                        | fleet_notification_bit(FleetNotificationKind::Docked)
                                        | fleet_notification_bit(FleetNotificationKind::MinerOpc);

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

bool state_requires_enabled_follow_through(FleetState state)
{ return state_requires_follow_through(s_enabled_notifications, state); }

static_assert(state_requires_follow_through(fleet_notification_bit(FleetNotificationKind::ArrivedInSystem),
                                            FleetState::Warping));
static_assert(!state_requires_follow_through(fleet_notification_bit(FleetNotificationKind::ArrivedInSystem),
                                             FleetState::Impulsing));
static_assert(state_requires_follow_through(fleet_notification_bit(FleetNotificationKind::ArrivedAtDestination),
                                            FleetState::Impulsing));
static_assert(state_requires_follow_through(fleet_notification_bit(FleetNotificationKind::StartedMining),
                                            FleetState::Battling));
static_assert(state_requires_follow_through(fleet_notification_bit(FleetNotificationKind::MinerOpc),
                                            FleetState::Battling));
static_assert(!state_requires_follow_through(fleet_notification_bit(FleetNotificationKind::ArrivedInSystem),
                                             FleetState::Battling));
static_assert(state_requires_follow_through(fleet_notification_bit(FleetNotificationKind::RepairComplete),
                                            FleetState::Repairing));

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
    case FleetState::Deployed:
    case FleetState::CanLocate:
      return true;
    default:
      return false;
  }
}

constexpr bool arrived_in_system(FleetState old_state, FleetState new_state)
{ return old_state == FleetState::Warping && new_state == FleetState::Impulsing; }

constexpr bool arrived_at_destination(FleetState old_state, FleetState new_state)
{ return old_state == FleetState::Impulsing && state_is_destination(new_state); }

constexpr bool started_mining(FleetState old_state, FleetState new_state)
{ return old_state != FleetState::Mining && new_state == FleetState::Mining; }

constexpr bool docked(FleetState old_state, FleetState new_state)
{
  return old_state != FleetState::Repairing && new_state == FleetState::Docked && state_can_dock_from_space(old_state);
}

constexpr bool repair_complete(FleetState old_state, FleetState new_state)
{ return old_state == FleetState::Repairing && new_state == FleetState::Docked; }

constexpr std::array kTransitionRules{
    FleetTransitionRule{FleetNotificationKind::ArrivedInSystem, arrived_in_system},
    FleetTransitionRule{FleetNotificationKind::ArrivedAtDestination, arrived_at_destination},
    FleetTransitionRule{FleetNotificationKind::StartedMining, started_mining},
    FleetTransitionRule{FleetNotificationKind::Docked, docked},
    FleetTransitionRule{FleetNotificationKind::RepairComplete, repair_complete},
};

static_assert(arrived_in_system(FleetState::Warping, FleetState::Impulsing));
static_assert(arrived_at_destination(FleetState::Impulsing, FleetState::IdleInSpace));
static_assert(started_mining(FleetState::IdleInSpace, FleetState::Mining));
static_assert(docked(FleetState::Impulsing, FleetState::Docked));
static_assert(!docked(FleetState::Repairing, FleetState::Docked));
static_assert(repair_complete(FleetState::Repairing, FleetState::Docked));

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

std::string ship_name(FleetPlayerData* fleet)
{
  auto* hull = fleet ? fleet->Hull : nullptr;
  return hull && hull->Name ? normalize_name(to_string(hull->Name)) : std::string{};
}

std::string resource_name(int64_t resource_id)
{
  if (resource_id == 0) {
    return {};
  }

  auto*                      manager = SpecManager::Instance();
  auto*                      spec    = manager ? manager->GetResourceSpec(resource_id) : nullptr;
  auto                       name    = spec && spec->Name ? normalize_name(to_string(spec->Name)) : std::string{};
  constexpr std::string_view resource_prefix = "Resource ";
  if (name.starts_with(resource_prefix)) {
    name.erase(0, resource_prefix.size());
  }
  return name;
}

std::string notification_subject(FleetPlayerData* fleet)
{
  auto name = ship_name(fleet);
  return name.empty() ? "fleet" : name;
}

std::string cargo_text(FleetPlayerData* fleet)
{
  const auto fill = fleet ? fleet->CargoResourceFillLevel : -1.0f;
  if (!std::isfinite(fill) || fill < 0.0f) {
    return {};
  }
  const auto percent = static_cast<int>(std::lround(std::clamp(fill, 0.0f, 1.0f) * 100.0f));
  return " Current cargo: " + std::to_string(percent) + "%.";
}

FleetPlayerData* fleet_for_slot(int slot_index)
{
  auto* manager = FleetsManager::Instance();
  return manager && slot_index >= 0 && slot_index < kFleetSlotCount ? manager->GetFleetPlayerData(slot_index) : nullptr;
}

FleetPlayerData* fleet_for_snapshot(int slot_index, uint64_t fleet_id)
{
  auto* fleet = fleet_for_slot(slot_index);
  return fleet && fleet->Id == fleet_id ? fleet : nullptr;
}

void emit_transition(FleetNotificationKind kind, FleetPlayerData* fleet, int64_t resource_id)
{
  const auto subject = notification_subject(fleet);
  switch (kind) {
    case FleetNotificationKind::ArrivedInSystem:
      notification_emit("Fleet Arrived", "Your " + subject + " has arrived in-system");
      break;
    case FleetNotificationKind::ArrivedAtDestination:
      notification_emit("Fleet Arrived", "Your " + subject + " has reached its destination");
      break;
    case FleetNotificationKind::StartedMining: {
      const auto resource = resource_name(resource_id);
      const auto detail   = resource.empty() ? std::string{} : " " + resource;
      notification_emit("Fleet Mining", "Your " + subject + " started mining" + detail);
      break;
    }
    case FleetNotificationKind::Docked:
      notification_emit("Fleet Docked", "Your " + subject + " docked");
      break;
    case FleetNotificationKind::RepairComplete:
      notification_emit("Repair Complete", "Your " + subject + " finished repairs");
      break;
    default:
      break;
  }
}

constexpr bool cargo_is_opc(double current_value, double protected_limit)
{ return current_value > protected_limit; }

static_assert(!cargo_is_opc(99.0, 100.0));
static_assert(!cargo_is_opc(100.0, 100.0));
static_assert(cargo_is_opc(101.0, 100.0));

bool read_opc(FleetPlayerData* fleet, bool& known)
{
  known             = false;
  auto* cargo_hold  = fleet ? fleet->CargoHoldData : nullptr;
  auto* unprotected = cargo_hold ? cargo_hold->UnprotectedCargoProgress : nullptr;
  if (!unprotected) {
    return false;
  }

  const auto current_value   = unprotected->CurrentValue;
  const auto protected_limit = unprotected->MinValue;
  if (!std::isfinite(current_value) || !std::isfinite(protected_limit)) {
    return false;
  }

  known = true;
  return cargo_is_opc(current_value, protected_limit);
}

bool allow_node_depleted_notification(uint64_t fleet_id)
{
  const auto              now    = now_milliseconds();
  FleetNotificationStamp* oldest = &s_node_depleted_stamps.front();
  for (auto& stamp : s_node_depleted_stamps) {
    if (stamp.occupied && stamp.fleet_id == fleet_id) {
      if (now - stamp.last_emitted_ms < kDuplicateSuppressionMs) {
        return false;
      }
      stamp.last_emitted_ms = now;
      return true;
    }
    if (!stamp.occupied) {
      oldest = &stamp;
      break;
    }
    if (stamp.last_emitted_ms < oldest->last_emitted_ms) {
      oldest = &stamp;
    }
  }

  *oldest = FleetNotificationStamp{fleet_id, now, true};
  return true;
}

int resolve_slot(FleetPlayerData* fleet, int requested_slot)
{
  if (!fleet) {
    return -1;
  }
  if (requested_slot >= 0 && requested_slot < kFleetSlotCount) {
    return requested_slot;
  }

  for (int index = 0; index < kFleetSlotCount; ++index) {
    if (s_slots[index].occupied && s_slots[index].fleet_id == fleet->Id) {
      return index;
    }
  }

  const auto model_slot = fleet->Index;
  if (model_slot >= 0 && model_slot < kFleetSlotCount) {
    return model_slot;
  }
  return -1;
}

void observe_fleet(FleetPlayerData* fleet, int requested_slot, bool allow_notifications, bool sample_opc,
                   std::string_view source)
{
  const auto slot_index = resolve_slot(fleet, requested_slot);
  if (!fleet || slot_index < 0) {
    return;
  }

  auto&      previous   = s_slots[slot_index];
  const auto fleet_id   = fleet->Id;
  const auto current    = fleet->CurrentState;
  const bool same_fleet = previous.occupied && previous.fleet_id == fleet_id;
  if (!same_fleet && !s_seed_pending && !s_seed_restart_disabled) {
    restart_seed_stabilization(slot_index, fleet_id);
  }
  allow_notifications                 = allow_notifications && !s_seed_pending;
  const bool resource_context_enabled = notification_enabled(FleetNotificationKind::StartedMining)
                                        || notification_enabled(FleetNotificationKind::NodeDepleted);
  auto*      mining_data = resource_context_enabled && current == FleetState::Mining ? fleet->MiningData : nullptr;
  const auto resource_id = mining_data ? mining_data->ResourceId : (same_fleet ? previous.resource_id : 0);
  const bool miner_opc_enabled = notification_enabled(FleetNotificationKind::MinerOpc);
  bool       opc_known         = same_fleet && previous.state == FleetState::Mining ? previous.opc_known : false;
  bool       opc               = same_fleet && previous.state == FleetState::Mining ? previous.opc : false;
  if (miner_opc_enabled && current == FleetState::Mining
      && (sample_opc || !same_fleet || previous.state != FleetState::Mining)) {
    bool       sampled_known = false;
    const bool sampled_opc   = read_opc(fleet, sampled_known);
    if (sampled_known) {
      opc_known = true;
      opc       = sampled_opc;
    }
  } else if (current != FleetState::Mining) {
    opc_known = false;
    opc       = false;
  }

  const bool previously_watched =
      miner_opc_enabled && previous.occupied && previous.state == FleetState::Mining && !previous.opc;
  const bool previously_following = previous.occupied && previous.follow_through_started_ms != 0;
  const bool seed_state_changed   = !same_fleet || previous.state != current || previous.resource_id != resource_id
                                    || previous.opc_known != opc_known || previous.opc != opc;
  if (!same_fleet) {
    s_last_emitted_ms[slot_index] = {};
  }
  if (same_fleet && allow_notifications && previous.state != current) {
    for (const auto& rule : kTransitionRules) {
      if (notification_enabled(rule.kind) && rule.matches(previous.state, current)) {
        if (!allow_notification(slot_index, rule.kind)) {
          spdlog::debug("[FleetWatch] source={} suppressed duplicate event={} slot={} fleet={}", source,
                        kFleetNotificationCatalog[static_cast<std::size_t>(rule.kind)].config_name, slot_index,
                        fleet_id);
          continue;
        }
        spdlog::debug("[FleetWatch] source={} event={} slot={} fleet={} oldState={} newState={}", source,
                      kFleetNotificationCatalog[static_cast<std::size_t>(rule.kind)].config_name, slot_index, fleet_id,
                      static_cast<int>(previous.state), static_cast<int>(current));
        emit_transition(rule.kind, fleet, resource_id);
      }
    }
  }

  if (same_fleet && allow_notifications && miner_opc_enabled && previous.state == FleetState::Mining
      && current == FleetState::Mining && previous.opc_known && opc_known && !previous.opc && opc) {
    if (!allow_notification(slot_index, FleetNotificationKind::MinerOpc)) {
      spdlog::debug("[FleetWatch] source={} suppressed duplicate event=MinerOPC slot={} fleet={}", source, slot_index,
                    fleet_id);
    } else {
      const auto subject = notification_subject(fleet);
      spdlog::debug("[FleetWatch] source={} event=MinerOPC slot={} fleet={}", source, slot_index, fleet_id);
      notification_emit("Miner Is OPC", "Your " + subject + " is now over protected cargo." + cargo_text(fleet));
    }
  }

  int64_t follow_through_started_ms = 0;
  if (state_requires_enabled_follow_through(current)) {
    if (same_fleet && state_requires_enabled_follow_through(previous.state)) {
      follow_through_started_ms = previous.follow_through_started_ms;
    } else {
      follow_through_started_ms     = now_milliseconds();
      s_last_follow_through_scan_ms = 0;
      spdlog::debug("[FleetWatch] follow-through scan armed source={} slot={} fleet={}", source, slot_index, fleet_id);
    }
  }

  previous = FleetSnapshot{fleet_id, resource_id, current, true, opc_known, opc, follow_through_started_ms};
  const bool currently_watched = miner_opc_enabled && current == FleetState::Mining && !opc;
  if (previously_watched != currently_watched) {
    if (currently_watched && s_mining_watch_count == 0) {
      s_last_mining_scan_ms = now_milliseconds();
    }
    s_mining_watch_count += currently_watched ? 1 : -1;
  }
  const bool currently_following = follow_through_started_ms != 0;
  if (previously_following != currently_following) {
    s_follow_through_watch_count += currently_following ? 1 : -1;
  }
  if (s_seed_pending && s_seed_has_observation && seed_state_changed) {
    s_seed_last_change_ms = now_milliseconds();
  }
  if (s_seed_pending && seed_state_changed) {
    s_seed_ready_candidate = false;
  }
}

struct ScanResult {
  int  observed_count    = 0;
  bool manager_available = false;
};

ScanResult scan_fleets(bool allow_notifications, bool sample_opc, std::string_view source)
{
  ScanResult result;
  auto*      manager = FleetsManager::Instance();
  if (!manager) {
    return result;
  }
  result.manager_available = true;
  s_seed_restart_disabled  = false;

  std::array<FleetPlayerData*, kFleetSlotCount> fleets{};
  for (int index = 0; index < kFleetSlotCount; ++index) {
    fleets[index] = manager->GetFleetPlayerData(index);
  }

  if (allow_notifications && !s_seed_pending) {
    for (int index = 0; index < kFleetSlotCount; ++index) {
      auto* fleet = fleets[index];
      if (!fleet) {
        continue;
      }
      const auto fleet_id = fleet->Id;
      if (!s_slots[index].occupied || s_slots[index].fleet_id != fleet_id) {
        restart_seed_stabilization(index, fleet_id);
        allow_notifications = false;
        break;
      }
    }
  }

  for (int index = 0; index < kFleetSlotCount; ++index) {
    auto* fleet = fleets[index];
    if (!fleet) {
      if (s_seed_pending && s_seed_has_observation && s_slots[index].occupied) {
        s_seed_last_change_ms  = now_milliseconds();
        s_seed_ready_candidate = false;
      }
      if (notification_enabled(FleetNotificationKind::MinerOpc) && s_slots[index].occupied
          && s_slots[index].state == FleetState::Mining && !s_slots[index].opc) {
        --s_mining_watch_count;
      }
      if (s_slots[index].occupied && s_slots[index].follow_through_started_ms != 0) {
        --s_follow_through_watch_count;
      }
      s_slots[index]           = {};
      s_last_emitted_ms[index] = {};
      continue;
    }

    observe_fleet(fleet, index, allow_notifications, sample_opc, source);
    ++result.observed_count;
  }
  return result;
}
} // namespace

bool fleet_watch_uses_state_observation()
{ return s_state_observation_enabled; }

bool fleet_watch_uses_node_depleted_hook()
{ return notification_enabled(FleetNotificationKind::NodeDepleted); }

void fleet_watch_init()
{
  constexpr auto follow_through_events = fleet_notification_bit(FleetNotificationKind::ArrivedInSystem)
                                         | fleet_notification_bit(FleetNotificationKind::ArrivedAtDestination)
                                         | fleet_notification_bit(FleetNotificationKind::StartedMining)
                                         | fleet_notification_bit(FleetNotificationKind::Docked)
                                         | fleet_notification_bit(FleetNotificationKind::RepairComplete);
  constexpr auto state_events   = follow_through_events | fleet_notification_bit(FleetNotificationKind::MinerOpc);
  s_enabled_notifications       = Config::Get().notify_fleet_events;
  s_state_observation_enabled   = (s_enabled_notifications & state_events) != 0;
  s_slots                       = {};
  s_last_emitted_ms             = {};
  s_node_depleted_stamps        = {};
  s_seed_pending                = fleet_watch_uses_state_observation();
  s_seed_has_observation        = false;
  s_seed_ready_candidate        = false;
  s_seed_candidate_forced       = false;
  s_seed_restart_disabled       = false;
  s_last_follow_through_scan_ms = 0;
  s_last_mining_scan_ms         = 0;
  s_last_seed_attempt_ms        = 0;
  s_seed_started_ms             = 0;
  s_seed_first_observed_ms      = 0;
  s_seed_last_change_ms         = 0;
  s_seed_candidate_count        = 0;
  s_mining_watch_count          = 0;
  s_follow_through_watch_count  = 0;
}

void fleet_watch_observe_widget(FleetPlayerData* fleet)
{
  if (!fleet || !fleet_watch_uses_state_observation()) {
    return;
  }

  observe_fleet(fleet, -1, !s_seed_pending, false, "fleet-state-widget");
}

void fleet_watch_observe_node_depleted(int64_t fleet_id)
{
  if (!fleet_watch_uses_node_depleted_hook()) {
    return;
  }

  const auto fleet_key = static_cast<uint64_t>(fleet_id);
  if (!allow_node_depleted_notification(fleet_key)) {
    spdlog::debug("[FleetWatch] suppressed duplicate event=NodeDepleted fleet={}", fleet_key);
    return;
  }

  for (int index = 0; index < kFleetSlotCount; ++index) {
    const auto& snapshot = s_slots[index];
    if (!snapshot.occupied || snapshot.fleet_id != fleet_key) {
      continue;
    }

    auto*      fleet    = fleet_for_snapshot(index, snapshot.fleet_id);
    const auto subject  = notification_subject(fleet);
    const auto resource = resource_name(snapshot.resource_id);
    const auto detail   = resource.empty() ? std::string{" its node"} : " its " + resource + " node";
    notification_emit("Node Depleted", subject + " depleted" + detail + "." + cargo_text(fleet));
    return;
  }

  for (int index = 0; index < kFleetSlotCount; ++index) {
    auto* fleet = fleet_for_slot(index);
    if (!fleet || fleet->Id != fleet_key) {
      continue;
    }

    auto*      mining_data = fleet->MiningData;
    const auto resource    = resource_name(mining_data ? mining_data->ResourceId : 0);
    const auto detail      = resource.empty() ? std::string{" its node"} : " its " + resource + " node";
    notification_emit("Node Depleted", notification_subject(fleet) + " depleted" + detail + "." + cargo_text(fleet));
    return;
  }

  notification_emit("Node Depleted", "A mining fleet depleted its node.");
}

void fleet_watch_tick()
{
  if (!fleet_watch_uses_state_observation()) {
    return;
  }
  if (!s_seed_pending && s_follow_through_watch_count == 0 && s_mining_watch_count == 0) {
    return;
  }

  const auto now_ms = now_milliseconds();
  if (s_seed_pending) {
    if (s_seed_started_ms == 0) {
      s_seed_started_ms = now_ms;
    }
    const auto seed_lifetime = now_ms - s_seed_started_ms;
    if (seed_lifetime >= kSeedLifetimeMs && !s_seed_has_observation) {
      s_seed_pending                = false;
      s_seed_ready_candidate        = false;
      s_seed_restart_disabled       = true;
      s_slots                       = {};
      s_last_emitted_ms             = {};
      s_last_follow_through_scan_ms = 0;
      s_last_mining_scan_ms         = 0;
      s_mining_watch_count          = 0;
      s_follow_through_watch_count  = 0;
      spdlog::warn("[FleetWatch] initial seed stopped after {}ms without a stable fleet baseline", kSeedLifetimeMs);
    } else {
      const auto seed_interval =
          s_seed_has_observation || seed_lifetime < kSeedFastPeriodMs ? kSeedFastIntervalMs : kSeedBackoffIntervalMs;
      if (s_last_seed_attempt_ms == 0 || now_ms - s_last_seed_attempt_ms >= seed_interval) {
        s_last_seed_attempt_ms = now_ms;
        const auto seed        = scan_fleets(false, true, "initial-seed");
        if (!seed.manager_available || seed.observed_count == 0) {
          s_seed_has_observation   = false;
          s_seed_ready_candidate   = false;
          s_seed_candidate_forced  = false;
          s_seed_first_observed_ms = 0;
          s_seed_last_change_ms    = 0;
          s_seed_candidate_count   = 0;
        } else {
          if (!s_seed_has_observation) {
            s_seed_has_observation   = true;
            s_seed_first_observed_ms = now_ms;
            s_seed_last_change_ms    = now_ms;
            spdlog::debug("[FleetWatch] observed {} fleet slots; waiting for stable initial state",
                          seed.observed_count);
          } else {
            const bool quiet  = now_ms - s_seed_last_change_ms >= kSeedStabilizationMs;
            const bool forced = now_ms - s_seed_first_observed_ms >= kSeedMaxStabilizationMs;
            if (quiet || forced) {
              s_seed_ready_candidate  = true;
              s_seed_candidate_forced = forced && !quiet;
              s_seed_candidate_count  = seed.observed_count;
            }
          }
        }
      }
    }
  }

  if (s_seed_pending) {
    return;
  }

  bool follow_through_due  = false;
  bool follow_through_fast = false;
  for (int index = 0; index < kFleetSlotCount; ++index) {
    auto& slot = s_slots[index];
    if (!slot.occupied || slot.follow_through_started_ms == 0) {
      continue;
    }
    const auto lifetime = now_ms - slot.follow_through_started_ms;
    if (lifetime >= kFollowThroughLifetimeMs) {
      slot.follow_through_started_ms = 0;
      --s_follow_through_watch_count;
      spdlog::warn("[FleetWatch] follow-through scan expired slot={} fleet={} after 24 hours", index, slot.fleet_id);
    } else if (lifetime < kFollowThroughFastPeriodMs) {
      follow_through_fast = true;
    }
  }
  if (s_follow_through_watch_count > 0) {
    const auto interval = follow_through_fast ? kFollowThroughIntervalMs : kFollowThroughBackoffMs;
    follow_through_due  = s_last_follow_through_scan_ms == 0 || now_ms - s_last_follow_through_scan_ms >= interval;
  }

  const bool mining_due = s_mining_watch_count > 0
                          && (s_last_mining_scan_ms == 0 || now_ms - s_last_mining_scan_ms >= kMiningWatchIntervalMs);
  if (!follow_through_due && !mining_due) {
    return;
  }

  const bool had_follow_through = s_follow_through_watch_count > 0;
  scan_fleets(true, mining_due, follow_through_due ? "follow-through" : "mining-watch");
  if (follow_through_due) {
    s_last_follow_through_scan_ms = now_ms;
    if (had_follow_through && s_follow_through_watch_count == 0) {
      spdlog::debug("[FleetWatch] follow-through scan settled");
    }
  }
  if (mining_due) {
    s_last_mining_scan_ms = now_ms;
  }
}

void fleet_watch_after_update()
{
  if (!s_seed_pending || !s_seed_ready_candidate) {
    return;
  }

  const bool forced = s_seed_candidate_forced;
  const auto seed   = scan_fleets(false, true, "seed-finalize");
  if (!seed.manager_available || seed.observed_count == 0) {
    s_seed_has_observation   = false;
    s_seed_ready_candidate   = false;
    s_seed_candidate_forced  = false;
    s_seed_first_observed_ms = 0;
    s_seed_last_change_ms    = 0;
    s_seed_candidate_count   = 0;
    return;
  }
  if (!forced && !s_seed_ready_candidate) {
    return;
  }

  const auto now_ms             = now_milliseconds();
  s_seed_pending                = false;
  s_seed_ready_candidate        = false;
  s_seed_candidate_count        = seed.observed_count;
  s_last_follow_through_scan_ms = s_follow_through_watch_count > 0 ? now_ms : 0;
  s_last_mining_scan_ms         = s_mining_watch_count > 0 ? now_ms : 0;
  if (forced) {
    spdlog::warn("[FleetWatch] seeded {} fleet slots after {}ms without a quiet window", s_seed_candidate_count,
                 kSeedMaxStabilizationMs);
  } else {
    spdlog::debug("[FleetWatch] seeded {} stable fleet slots after {}ms quiet window", s_seed_candidate_count,
                  kSeedStabilizationMs);
  }
}
