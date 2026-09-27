#if __APPLE__
#include "patches/notification_desktop_mac.h"
#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>
#include <spdlog/spdlog.h>

namespace
{
UNUserNotificationCenter* Center()
{
  // The injected library uses the game's bundle identity. A command-line host
  // without an application bundle cannot register a notification center.
  if (![NSBundle mainBundle].bundleIdentifier.length) return nil;
  @try {
    return [UNUserNotificationCenter currentNotificationCenter];
  } @catch (NSException* exception) {
    spdlog::warn("[Notify] macOS notification center unavailable: {}", exception.name.UTF8String);
    return nil;
  }
}
}

void notification_desktop_mac_init()
{
  dispatch_async(dispatch_get_main_queue(), ^{
    @autoreleasepool {
      UNUserNotificationCenter* center = Center();
      if (!center) {
        spdlog::warn("[Notify] macOS desktop notifications require the game application bundle");
        return;
      }
      // Do not replace the game's delegate or add a second sound. Audio cues
      // have their own config and backend. macOS controls background banners.
      [center requestAuthorizationWithOptions:UNAuthorizationOptionAlert completionHandler:^(BOOL granted, NSError* error) {
        if (error)
          spdlog::warn("[Notify] macOS authorization failed: {}", error.localizedDescription.UTF8String);
        else
          spdlog::info("[Notify] macOS desktop notifications authorized={}", bool(granted));
      }];
    }
  });
}

void notification_desktop_mac_emit(std::string_view title, std::string_view body)
{
  @autoreleasepool {
    NSString* ownedTitle = [[NSString alloc] initWithBytes:title.data() length:title.size() encoding:NSUTF8StringEncoding];
    NSString* ownedBody = [[NSString alloc] initWithBytes:body.data() length:body.size() encoding:NSUTF8StringEncoding];
    if (ownedTitle && ownedBody) {
      dispatch_async(dispatch_get_main_queue(), ^{
        @autoreleasepool {
          UNUserNotificationCenter* center = Center();
          if (!center) return;
          [center getNotificationSettingsWithCompletionHandler:^(UNNotificationSettings* settings) {
            @autoreleasepool {
              if (settings.authorizationStatus != UNAuthorizationStatusAuthorized
                  && settings.authorizationStatus != UNAuthorizationStatusProvisional) return;
              UNMutableNotificationContent* content = [[UNMutableNotificationContent alloc] init];
              content.title = ownedTitle;
              content.body = ownedBody;
              UNNotificationRequest* request = [UNNotificationRequest requestWithIdentifier:[NSUUID UUID].UUIDString
                                                                                  content:content trigger:nil];
              [center addNotificationRequest:request withCompletionHandler:^(NSError* error) {
                if (error)
                  spdlog::warn("[Notify] macOS delivery failed: {}", error.localizedDescription.UTF8String);
              }];
#if !__has_feature(objc_arc)
              [content release];
#endif
            }
          }];
        }
      });
    }
#if !__has_feature(objc_arc)
    [ownedTitle release];
    [ownedBody release];
#endif
  }
}
#endif
