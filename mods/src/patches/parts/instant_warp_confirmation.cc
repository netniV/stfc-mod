#include "config.h"
#include "errormsg.h"
#include "str_utils.h"

#include <prime/CourseData.h>
#include <prime/CoursePromptPopupWidget.h>

#include <spud/detour.h>

#include <algorithm>

namespace
{
using PopupAction = void(CoursePromptPopupWidget*);
PopupAction* initiate_regular_warp        = nullptr;
PopupAction* on_instant_warp_button_click = nullptr;

void CoursePromptPopupViewController_AboutToShow_Hook(auto original, CoursePromptPopupViewController* _this)
{
  original(_this);

  const auto widget  = _this == nullptr ? nullptr : _this->PopupWidget;
  const auto context = widget == nullptr ? nullptr : widget->Context;
  if (context == nullptr || !context->HasInstantWarp) {
    return;
  }

  const auto& cfg = Config::Get();

  std::string hull_name;
  if (const auto course = context->GetCourseData(); course != nullptr) {
    if (const auto fleet = course->PlayerFleet; fleet != nullptr) {
      if (const auto hull = fleet->Hull; hull != nullptr) {
        if (const auto name = hull->Name; name != nullptr) {
          hull_name = AsciiStrToUpper(to_string(name));
          hull_name = StripSuffix(hull_name, "_LIVE");
        }
      }
    }
  }

  const auto matches = [&hull_name](const std::vector<std::string>& names, bool all) -> bool {
    if (all) return true;
    if (hull_name.empty()) return false;
    return std::ranges::any_of(names, [&](const auto& s) { return s == hull_name; });
  };

  if (matches(cfg.instant_warp_always_ask, cfg.instant_warp_always_ask_all)) {
    return;
  }
  if (matches(cfg.instant_warp_auto_jump, cfg.instant_warp_auto_jump_all)) {
    on_instant_warp_button_click(widget);
    return;
  }
  if (matches(cfg.instant_warp_auto_warp, cfg.instant_warp_auto_warp_all)) {
    initiate_regular_warp(widget);
    return;
  }

  switch (cfg.auto_confirm_instant_warp) {
    case InstantWarpConfirmation::Warp:
      initiate_regular_warp(widget);
      break;
    case InstantWarpConfirmation::Jump:
      on_instant_warp_button_click(widget);
      break;
    case InstantWarpConfirmation::None:
      break;
  }
}
} // namespace

void InstallInstantWarpConfirmationHooks()
{
  auto helper = CoursePromptPopupViewController::get_class_helper();
  if (!helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.ObjectViewer", "CoursePromptPopupViewController");
    return;
  }

  const auto about_to_show = helper.GetMethod("AboutToShow", 0);
  if (about_to_show == nullptr) {
    ErrorMsg::MissingMethod("CoursePromptPopupViewController", "AboutToShow");
    return;
  }

  auto widget_helper = CoursePromptPopupWidget::get_class_helper();
  if (!widget_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.Navigation", "CoursePromptPopupWidget");
    return;
  }

  initiate_regular_warp = widget_helper.GetMethod<PopupAction>("InitiateRegularWarp", 0);
  if (initiate_regular_warp == nullptr) {
    ErrorMsg::MissingMethod("CoursePromptPopupWidget", "InitiateRegularWarp");
    return;
  }

  on_instant_warp_button_click = widget_helper.GetMethod<PopupAction>("OnInstantWarpButtonClick", 0);
  if (on_instant_warp_button_click == nullptr) {
    ErrorMsg::MissingMethod("CoursePromptPopupWidget", "OnInstantWarpButtonClick");
    return;
  }

  SPUD_STATIC_DETOUR(about_to_show, CoursePromptPopupViewController_AboutToShow_Hook);
}
