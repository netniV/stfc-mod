#pragma once

enum class SectionID : int;

struct ScreenManager;
class RewardsButtonWidget;
struct PreScanTargetWidget;

void hotkey_router_init();
bool hotkey_router_screen_update(ScreenManager* screen_manager);
bool hotkey_router_init_actions();
void hotkey_router_bind_context(RewardsButtonWidget* widget);
void hotkey_router_show_fleet(PreScanTargetWidget* widget);
void hotkey_router_goto_section(SectionID section_id, void* section_data = nullptr);
void hotkey_router_change_navigation_section(SectionID section_id);