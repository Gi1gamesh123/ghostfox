#ifndef RUYIJS_WEB_PERMISSIONS_INTERNAL_H
#define RUYIJS_WEB_PERMISSIONS_INTERNAL_H
#include "web_permissions/realm.h"
#include "runtime/web_kit.h"
#include <array>
#include <cstdint>
#include <memory>

namespace ruyijs::web_permissions {
// PermissionName in Firefox 157's Permissions.webidl order.
inline constexpr const char* kNames[] = {"geolocation", "notifications", "push",
    "persistent-storage", "midi", "storage-access", "screen-wake-lock", "camera", "microphone",
    "loopback-network", "local-network"};
inline constexpr size_t kNameCount = sizeof(kNames) / sizeof(kNames[0]);
inline constexpr const char* kStates[] = {"granted", "denied", "prompt"};
// The Context's one permission-state table. Defaults are a fresh Firefox 157
// profile (artifacts/batch-gap-firefox, permissions-network-firefox): prompt,
// except screen-wake-lock granted.
struct Table {
  std::array<uint8_t, kNameCount> states{2, 2, 2, 2, 2, 2, 0, 2, 2, 2, 2};
};
enum StatusSlot { StatusName = 0, StatusIndex = 1, StatusHandler = 2, StatusSlotCount = 3 };
// Notification: ten field slots in member order, four handler slots
// (click/show/error/close) and the closed flag.
enum NoteSlot { NoteFieldCount = 10, NoteClosed = 14, NoteSlotCount = 15 };
extern const JSClass kPermissionsClass, kStatusClass, kNotificationClass;
struct State {
  State(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, interaction::Realm& events)
      : cx(cx), kit(cx, page, helpers, this), events(events), permissions(cx), status(cx),
        notification(cx), instance(cx) {}
  JSContext* cx;
  WebKit kit;
  interaction::Realm& events;
  WebFace permissions, status, notification;
  JS::PersistentRootedObject instance;
  // Shared with every frame realm: a retired frame may outlive the main realm.
  std::shared_ptr<Table> table;
  // Scheduling port for Notification events, bound with the window message ports.
  Realm::TaskPort taskPort = nullptr;
  void* taskContext = nullptr;
};
bool Query(JSContext* cx, unsigned argc, JS::Value* vp);
bool NotificationCtor(JSContext* cx, unsigned argc, JS::Value* vp);
const char* NotificationPermission(State& state);
bool QueueNotificationEvent(State& state, JS::HandleObject note, const char* type);
bool InstallNotification(State& state);
bool StatusGet(JSContext* cx, unsigned argc, JS::Value* vp);
bool StatusHandlerSet(JSContext* cx, unsigned argc, JS::Value* vp);
}
#endif
