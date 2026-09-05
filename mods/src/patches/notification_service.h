#pragma once

#include <string_view>

struct Toast;

// Temporarily prevent game toasts from being forwarded as system notifications.
// The in-game toast pipeline is unaffected, and suppression is local to the calling thread.
class ScopedToastNotificationSuppression
{
public:
  ScopedToastNotificationSuppression();
  ~ScopedToastNotificationSuppression();

  ScopedToastNotificationSuppression(const ScopedToastNotificationSuppression&)            = delete;
  ScopedToastNotificationSuppression& operator=(const ScopedToastNotificationSuppression&) = delete;
};

// Initialize the notification service (resolve IL2CPP methods, init platform).
// Idempotent; call from each feature installer that emits notifications.
void notification_init();

// Deliver a notification created by a mod feature rather than a Scopely toast.
void notification_emit(std::string_view title, std::string_view body);

// Called from toast hooks — checks config, formats, and delivers notification.
void notification_handle_toast(Toast* toast);
