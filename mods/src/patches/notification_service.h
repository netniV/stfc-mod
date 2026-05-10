#pragma once

struct Toast;

// Initialize the notification service (resolve IL2CPP methods, init platform).
// Call once during InstallToastBannerHooks().
void notification_init();

// Called from toast hooks — checks config, formats, and delivers notification.
void notification_handle_toast(Toast* toast);

/**
 * @brief Send an arbitrary OS-native notification with the given title and body.
 *
 * Requires notification_init() to have been called first. No-op on non-Windows.
 *
 * @param title Notification title text.
 * @param body  Notification body text.
 */
void notification_show(const char* title, const char* body);
