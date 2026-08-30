#pragma once

#include <string_view>

struct Toast;

// Initialize the notification service (resolve IL2CPP methods, init platform).
// Call once during InstallToastBannerHooks().
void notification_init();

// Deliver a notification created by a mod feature rather than a Scopely toast.
void notification_emit(std::string_view title, std::string_view body);

// Called from toast hooks — checks config, formats, and delivers notification.
void notification_handle_toast(Toast* toast);
