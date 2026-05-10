#pragma once

struct Toast;

// Initialize the notification service (resolve IL2CPP methods, init platform).
// Call once during InstallToastBannerHooks().
void notification_init();

// Called from toast hooks — checks config, formats, and delivers notification.
void notification_handle_toast(Toast* toast);
