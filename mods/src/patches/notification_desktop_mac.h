#pragma once
#include <string_view>

void notification_desktop_mac_init();
void notification_desktop_mac_emit(std::string_view title, std::string_view body);
