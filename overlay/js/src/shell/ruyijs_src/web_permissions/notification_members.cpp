// Notification members and statics, and its installer (PERMISSIONS.md slice 2).
#include "web_permissions/internal.h"
#include "interaction/realm.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/ValueArray.h"
#include "js/Wrapper.h"
#include <string>

namespace ruyijs::web_permissions {
namespace {
const char* const kFields[NoteFieldCount] = {"title", "dir", "lang", "body", "tag", "icon",
    "requireInteraction", "silent", "data", "actions"};
const char* const kHandlers[4] = {"click", "show", "error", "close"};
State& Self(const JS::CallArgs& args) { return *static_cast<State*>(WebKit::Owner(args)); }
JSObject* Note(JSContext* cx, JS::HandleValue self, const std::string& member) {
  JSObject* object = self.isObject() ? js::CheckedUnwrapStatic(&self.toObject()) : nullptr;
  if (object && JS::GetClass(object) == &kNotificationClass) return object;
  WebKit::Fail(cx, ("'" + member + "' called on an object that does not implement interface "
                    "Notification.").c_str());
  return nullptr;
}
bool FieldGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = WebKit::Slot(args);
  JS::RootedObject self(cx, Note(cx, args.thisv(), std::string("get ") + kFields[slot]));
  if (!self) return false;
  args.rval().set(JS::GetReservedSlot(self, slot));
  return JS_WrapValue(cx, args.rval());
}
bool HandlerGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t which = WebKit::Slot(args);
  JS::RootedObject self(cx, Note(cx, args.thisv(), std::string("get on") + kHandlers[which]));
  if (!self) return false;
  args.rval().set(JS::GetReservedSlot(self, NoteFieldCount + which));
  return JS_WrapValue(cx, args.rval());
}
bool HandlerSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = Self(args);
  const int32_t which = WebKit::Slot(args);
  JS::RootedObject self(cx, Note(cx, args.thisv(), std::string("set on") + kHandlers[which]));
  if (!self) return false;
  JS::RootedValue previous(cx, JS::GetReservedSlot(self, NoteFieldCount + which));
  JS::RootedValue desired(cx, args.get(0));
  if (!desired.isObject() || !JS::IsCallable(&desired.toObject())) desired.setNull();
  JSAutoRealm realm(cx, state.kit.page);
  if (!JS_WrapValue(cx, &desired) || !JS_WrapValue(cx, &previous)) return false;
  JS::RootedObject prototype(cx, state.events.prototype());
  JS::RootedValue function(cx), receiver(cx, JS::ObjectValue(*self)), ignored(cx);
  JS::RootedString type(cx, JS_NewStringCopyZ(cx, kHandlers[which]));
  if (!type) return false;
  JS::RootedValueArray<2> parameters(cx);
  parameters[0].setString(type);
  for (int operation = 0; operation < 2; ++operation) {
    parameters[1].set(operation ? desired : previous);
    if (!parameters[1].isObject()) continue;
    if (!JS_GetProperty(cx, prototype, operation ? "addEventListener" : "removeEventListener",
                        &function) ||
        !JS::Call(cx, receiver, function, parameters, &ignored)) return false;
  }
  JS::SetReservedSlot(self, NoteFieldCount + which, desired);
  args.rval().setUndefined();
  return true;
}
// A displayed (permitted) notification fires `close`; one never shown does not.
bool CloseNote(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = Self(args);
  JS::RootedObject self(cx, Note(cx, args.thisv(), "close"));
  if (!self) return false;
  args.rval().setUndefined();
  if (std::string(NotificationPermission(state)) != "granted" ||
      JS::GetReservedSlot(self, NoteClosed).isTrue()) return true;
  JS::SetReservedSlot(self, NoteClosed, JS::TrueValue());
  return QueueNotificationEvent(state, self, "close");
}
bool Permission(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = Self(args);
  if (WebKit::Slot(args) == 1) { args.rval().setInt32(2); return true; }  // maxActions
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, NotificationPermission(state)));
  if (!text) return false;
  args.rval().setString(text);
  return true;
}
// Without user activation Firefox answers with the current permission and
// does not prompt; the optional callback receives the same value.
bool RequestPermission(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = Self(args);
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, NotificationPermission(state)));
  if (!text) return false;
  JS::RootedValue value(cx, JS::StringValue(text)), callback(cx, args.get(0));
  if (!callback.isUndefined() && (!callback.isObject() || !JS::IsCallable(&callback.toObject())))
    return WebKit::Fail(cx, "Notification.requestPermission: Argument 1 is not callable.");
  JS::RootedValue promise(cx);
  if (!state.kit.Resolve(value, &promise)) return false;
  if (callback.isObject()) {
    JS::RootedObject then(cx, &promise.toObject());
    JS::RootedValue ignored(cx), method(cx);
    JS::RootedValueArray<1> arguments(cx);
    arguments[0].set(callback);
    if (!JS_GetProperty(cx, then, "then", &method) ||
        !JS::Call(cx, promise, method, arguments, &ignored)) return false;
  }
  args.rval().set(promise);
  return true;
}
}
bool InstallNotification(State& s) {
  JSContext* cx = s.cx;
  JS::RootedObject eventTarget(cx, s.events.prototype());
  WebFace& face = s.notification;
  if (!eventTarget ||
      !s.kit.Define("Notification", &kNotificationClass, eventTarget, NotificationCtor, &face) ||
      !JS_DefineProperty(cx, face.constructor, "length", 1, JSPROP_READONLY) ||
      !s.kit.Method(face, "Notification", "close", CloseNote, 0, 0)) return false;
  for (int32_t which = 0; which < 4; ++which)
    if (!s.kit.Accessor(face, "Notification", (std::string("on") + kHandlers[which]).c_str(),
                        HandlerGet, HandlerSet, which)) return false;
  for (int32_t slot = 0; slot < NoteFieldCount; ++slot)
    if (!s.kit.Accessor(face, "Notification", kFields[slot], FieldGet, nullptr, slot)) return false;
  JS::RootedObject request(cx, s.kit.Function(RequestPermission, "requestPermission", 0, 0));
  JS::RootedObject permission(cx, s.kit.Function(Permission, "get permission", 0, 0));
  JS::RootedObject actions(cx, s.kit.Function(Permission, "get maxActions", 0, 1)), none(cx);
  JS::RootedValue constructor(cx, JS::ObjectValue(*face.constructor));
  return request && permission && actions &&
         JS_DefineProperty(cx, face.constructor, "requestPermission", request, JSPROP_ENUMERATE) &&
         JS_DefineProperty(cx, face.constructor, "permission", permission, none, JSPROP_ENUMERATE) &&
         JS_DefineProperty(cx, face.constructor, "maxActions", actions, none, JSPROP_ENUMERATE) &&
         s.kit.helpers->Member(request, "Notification", "requestPermission", "call") &&
         s.kit.helpers->Accessor("Notification", face.constructor, "permission") &&
         JS_DeleteProperty(cx, face.prototype, "constructor") &&
         JS_DefineProperty(cx, face.prototype, "constructor", constructor, 0);
}
}
