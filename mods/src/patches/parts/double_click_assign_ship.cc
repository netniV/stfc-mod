#include "config.h"
#include "errormsg.h"

#include "prime/AssignShipsWidget.h"
#include "prime/CanvasController.h"
#include "prime/ShipTileWidget.h"

#include <il2cpp/il2cpp_helper.h>
#include <spud/detour.h>

#include <chrono>

// Ship-assignment dock: double-clicking a ship tile presses the Assign
// button, instead of requiring select-then-click-Assign. Detected by timing
// since Unity's Button.onClick doesn't report click count; keyed off the
// tile's bound FleetPlayerData context so reused/pooled list rows don't
// misfire across different ships.

namespace {

constexpr auto kDoubleClickWindow = std::chrono::milliseconds(400);

FleetPlayerData*                      g_last_clicked_ship = nullptr;
std::chrono::steady_clock::time_point g_last_click_time{};

void PressAssignButton()
{
  for (auto widget : ObjectFinder<AssignShipsWidget>::GetAll()) {
    if (!widget) continue;

    auto canvas = GetCanvasControllerFromComponent(widget);
    if (!canvas || !canvas->Visible() || !widget->isActiveAndEnabled) continue;

    auto* buttonWrapper = widget->_assignButton;
    auto* buttonWidget  = buttonWrapper ? buttonWrapper->Widget : nullptr;
    auto* listener      = buttonWidget ? buttonWidget->SemaphoreListener : nullptr;
    auto* button        = listener ? listener->TheButton : nullptr;
    if (button) {
      button->Press();
    }
    return;
  }
}

void ShipTileWidget_HandleOnClick_Hook(auto original, ShipTileWidget* _this)
{
  original(_this);

  if (!Config::Get().double_click_to_assign_ship) return;

  auto* ship = _this ? _this->Context : nullptr;
  if (!ship) return;

  const auto now             = std::chrono::steady_clock::now();
  const bool is_double_click = ship == g_last_clicked_ship && (now - g_last_click_time) <= kDoubleClickWindow;

  g_last_clicked_ship = ship;
  g_last_click_time   = now;

  if (!is_double_click) return;

  g_last_clicked_ship = nullptr; // consume, so a triple/quadruple click doesn't re-trigger immediately
  PressAssignButton();
}

} // namespace

void InstallDoubleClickAssignShipHooks()
{
  auto helper = ShipTileWidget::get_class_helper();
  if (!helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.Ships", "ShipTileWidget");
    return;
  }

  auto method = helper.GetMethod("HandleOnClick", 0);
  if (!method) {
    ErrorMsg::MissingMethod("ShipTileWidget", "HandleOnClick");
    return;
  }

  SPUD_STATIC_DETOUR(method, ShipTileWidget_HandleOnClick_Hook);
}
