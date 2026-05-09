#include "errormsg.h"

#include "config.h"

#include "patches/cargo_display.h"
#include "patches/viewer_mgmt.h"

#include "prime/RewardsButtonWidget.h"

bool CheckShowCargo(RewardsButtonWidget* widget)
{
  if (!Config::Get().show_cargo_default) {
    return false;
  }

  if (!widget->Context) {
    return false;
  }

  const auto target_fleet_deployed = widget->Context->TargetFleetDeployedData;

  if (!target_fleet_deployed) {
    return Config::Get().show_station_cargo;
  }

  auto fleet_type = target_fleet_deployed->FleetType;
  if (fleet_type == DeployedFleetType::Player) {
    return Config::Get().show_player_cargo;
  }
  if (fleet_type == DeployedFleetType::Marauder) {
    if (auto hull = target_fleet_deployed->Hull; hull && hull->Type == HullType::ArmadaTarget) {
      return Config::Get().show_armada_cargo;
    }

    return Config::Get().show_hostile_cargo;
  }

  return false;
}

void HandleCargoBindContext(RewardsButtonWidget* widget)
{
  if (CheckShowCargo(widget)) {
    widget->_rewardsController->Show(true);
    SetInfoPending(1);
  }
}

void HandleCargoShowFleet(RewardsButtonWidget* rewards_button_widget)
{
  if (CheckShowCargo(rewards_button_widget)) {
    rewards_button_widget->_rewardsController->Show(true);
    SetInfoPending(1);
  }
}