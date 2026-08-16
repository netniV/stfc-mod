#include "config.h"
#include "errormsg.h"

#include <prime/CoursePromptPopupWidget.h>

#include <spud/detour.h>
#include <spdlog/spdlog.h>

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

  switch (Config::Get().auto_confirm_instant_warp) {
    case InstantWarpConfirmation::Warp:
      spdlog::debug("InstantWarpConfirmation: selecting regular warp during popup show");
      initiate_regular_warp(widget);
      break;
    case InstantWarpConfirmation::Jump:
      spdlog::debug("InstantWarpConfirmation: selecting instant warp during popup show");
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
