#include "errormsg.h"

#include "patches/viewer_mgmt.h"

#include "prime/AllianceStarbaseObjectViewerWidget.h"
#include "prime/AnimatedRewardsScreenViewController.h"
#include "prime/ArmadaObjectViewerWidget.h"
#include "prime/CelestialObjectViewerWidget.h"
#include "prime/EmbassyObjectViewer.h"
#include "prime/HousingObjectViewerWidget.h"
#include "prime/MiningObjectViewerWidget.h"
#include "prime/MissionsObjectViewerWidget.h"
#include "prime/PreScanTargetWidget.h"

namespace {

int show_info_pending = 0;

template <typename T>
inline bool CanHideViewersOfType()
{
  for (auto widget : ObjectFinder<T>::GetAll()) {
    const auto visible = widget && widget->_visibilityController != nullptr
                         && (widget->_visibilityController->_state == VisibilityState::Visible
                             || widget->_visibilityController->_state == VisibilityState::Show);
    if (visible) {
      return true;
    }
  }

  return false;
}

template <typename T>
inline bool DidHideViewersOfType()
{
  const auto objects = ObjectFinder<T>::GetAll();
  auto       did_hide = false;
  for (auto widget : objects) {
    if (!widget) {
      continue;
    }
    auto visibility_controller = widget->_visibilityController;
    if (!visibility_controller) {
      continue;
    }
    const auto visible = (visibility_controller->_state == VisibilityState::Visible
                          || visibility_controller->_state == VisibilityState::Show);
    if (visible) {
      widget->HideAllViewers();
      did_hide = true;
    }
  }

  return did_hide;
}

} // namespace

bool CanHideViewers()
{
  return (CanHideViewersOfType<AllianceStarbaseObjectViewerWidget>() || CanHideViewersOfType<ArmadaObjectViewerWidget>()
          || CanHideViewersOfType<CelestialObjectViewerWidget>() || CanHideViewersOfType<EmbassyObjectViewer>()
          || CanHideViewersOfType<HousingObjectViewerWidget>() || CanHideViewersOfType<MiningObjectViewerWidget>()
          || CanHideViewersOfType<MissionsObjectViewerWidget>() || CanHideViewersOfType<PreScanTargetWidget>()
          || CanHideViewersOfType<HousingObjectViewerWidget>());
}

bool DidHideViewers()
{
  return DidHideViewersOfType<AllianceStarbaseObjectViewerWidget>() || DidHideViewersOfType<ArmadaObjectViewerWidget>()
         || DidHideViewersOfType<CelestialObjectViewerWidget>() || DidHideViewersOfType<EmbassyObjectViewer>()
         || DidHideViewersOfType<HousingObjectViewerWidget>() || DidHideViewersOfType<MiningObjectViewerWidget>()
         || DidHideViewersOfType<MissionsObjectViewerWidget>() || DidHideViewersOfType<PreScanTargetWidget>()
         || DidHideViewersOfType<HousingObjectViewerWidget>();
}

bool TryDismissRewardsScreen()
{
  if (auto reward_controller = ObjectFinder<AnimatedRewardsScreenViewController>::Get(); reward_controller) {
    if (reward_controller->IsActive()) {
      reward_controller->GoBackToLastSection();
      return true;
    }
  }

  return false;
}

void HandleActionView()
{
  auto all_pre_scan_widgets = ObjectFinder<PreScanTargetWidget>::GetAll();

  for (auto& pre_scan_widget : all_pre_scan_widgets) {
    if (pre_scan_widget
        && (pre_scan_widget->_visibilityController->_state == VisibilityState::Visible
            || pre_scan_widget->_visibilityController->_state == VisibilityState::Show)) {
      auto rewards_widget = pre_scan_widget->_rewardsButtonWidget;
      if (rewards_widget->_rewardsController->_state != VisibilityState::Visible
          && rewards_widget->_rewardsController->_state != VisibilityState::Show) {
        show_info_pending = 5;
      } else {
        rewards_widget->_rewardsController->Hide();
      }
    }
  }
}

void TickInfoPending()
{
  if (show_info_pending <= 0) {
    return;
  }

  auto all_pre_scan_widgets = ObjectFinder<PreScanTargetWidget>::GetAll();

  for (auto& pre_scan_widget : all_pre_scan_widgets) {
    const auto pre_scan_visible = pre_scan_widget
                                  && (pre_scan_widget->_visibilityController->_state == VisibilityState::Visible
                                      || pre_scan_widget->_visibilityController->_state == VisibilityState::Show);
    if (pre_scan_visible) {
      auto       rewards_widget         = pre_scan_widget->_rewardsButtonWidget;
      const auto rewards_widget_visible = rewards_widget->_rewardsController->_state == VisibilityState::Visible
                                          || rewards_widget->_rewardsController->_state == VisibilityState::Show;
      if (!rewards_widget_visible) {
        rewards_widget->_rewardsController->Show(true);
      }
    }
  }
  show_info_pending -= 1;
}

void SetInfoPending(int frames)
{
  show_info_pending = frames;
}