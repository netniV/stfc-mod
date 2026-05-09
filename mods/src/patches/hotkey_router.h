#pragma once

struct ScreenManager;
class RewardsButtonWidget;
struct PreScanTargetWidget;

void hotkey_router_init();
bool hotkey_router_screen_update(ScreenManager* screen_manager);
bool hotkey_router_init_actions();
void hotkey_router_bind_context(RewardsButtonWidget* widget);
void hotkey_router_show_fleet(PreScanTargetWidget* widget);
