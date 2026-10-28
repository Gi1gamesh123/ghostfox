// Gamepad, GamepadButton, GamepadHapticActuator and GamepadPose
// (docs/owners/GAMEPAD.md). No device exists, so these interfaces never get an
// instance; an instance would keep each attribute in its reserved slots.
#include "web_gamepad/realm.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/Realm.h"
#include <iterator>
#include <string>

namespace ruyijs::web_gamepad {
struct State {
  State(JSContext* cx, JS::HandleObject page, PageHelpers& helpers)
      : cx(cx), kit(cx, page, helpers, this) {}
  JSContext* cx;
  WebKit kit;
};
namespace {
enum Kind { Get, Promised };
struct Member { const char* name; Kind kind; unsigned length; };
struct Interface { const char* name; const Member* members; size_t count; };
// Firefox prototype order (dom/webidl/Gamepad*.webidl).
const Member kGamepad[] = {{"id", Get, 0}, {"index", Get, 0}, {"mapping", Get, 0},
                           {"hand", Get, 0}, {"connected", Get, 0}, {"buttons", Get, 0},
                           {"axes", Get, 0}, {"timestamp", Get, 0}, {"pose", Get, 0},
                           {"hapticActuators", Get, 0}};
const Member kButton[] = {{"pressed", Get, 0}, {"touched", Get, 0}, {"value", Get, 0}};
const Member kHaptic[] = {{"pulse", Promised, 2}, {"type", Get, 0}};
const Member kPose[] = {{"hasOrientation", Get, 0}, {"hasPosition", Get, 0},
                        {"position", Get, 0}, {"linearVelocity", Get, 0},
                        {"linearAcceleration", Get, 0}, {"orientation", Get, 0},
                        {"angularVelocity", Get, 0}, {"angularAcceleration", Get, 0}};
const Interface kInterfaces[] = {{"Gamepad", kGamepad, std::size(kGamepad)},
                                 {"GamepadButton", kButton, std::size(kButton)},
                                 {"GamepadHapticActuator", kHaptic, std::size(kHaptic)},
                                 {"GamepadPose", kPose, std::size(kPose)}};
const JSClass kClasses[] = {
    {"Gamepad", JSCLASS_HAS_RESERVED_SLOTS(std::size(kGamepad))},
    {"GamepadButton", JSCLASS_HAS_RESERVED_SLOTS(std::size(kButton))},
    {"GamepadHapticActuator", JSCLASS_HAS_RESERVED_SLOTS(std::size(kHaptic))},
    {"GamepadPose", JSCLASS_HAS_RESERVED_SLOTS(std::size(kPose))}};
constexpr int32_t kStride = 64;  // function slot = interface * kStride + member index

bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return trace.Finish(WebKit::Fail(cx, "Illegal constructor."));
}
bool MemberNative(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  const int32_t slot = WebKit::Slot(args);
  const Interface& face = kInterfaces[slot / kStride];
  const Member& m = face.members[slot % kStride];
  JSObject* self = args.thisv().isObject() ? &args.thisv().toObject() : nullptr;
  if (!self || JS::GetClass(self) != &kClasses[slot / kStride]) {
    const std::string member = m.kind == Get ? std::string("get ") + m.name : std::string(m.name);
    const std::string text = "'" + member +
        "' called on an object that does not implement interface " + face.name + ".";
    (void)WebKit::Fail(cx, text.c_str());
    if (m.kind != Promised) return false;
    JS::RootedValue error(cx);
    if (!JS_GetPendingException(cx, &error)) return false;
    JS_ClearPendingException(cx);
    return s.kit.RejectWith(error, args.rval());
  }
  // Unreachable until a device creates an instance.
  args.rval().set(JS::GetReservedSlot(self, slot % kStride));
  return JS_WrapValue(cx, args.rval());
}
}  // namespace

Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers)
    : state_(std::make_unique<State>(cx, page, helpers)) {}
Realm::~Realm() = default;

bool Realm::Install() {
  State& s = *state_;
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.kit.page);
  JS::RootedObject object(cx, JS::GetRealmObjectPrototype(cx));
  if (!object) return false;
  for (size_t i = 0; i < std::size(kInterfaces); ++i) {
    const Interface& face = kInterfaces[i];
    WebFace web(cx);
    if (!s.kit.Define(face.name, &kClasses[i], object, Illegal, &web)) return false;
    for (size_t j = 0; j < face.count; ++j) {
      const Member& m = face.members[j];
      const int32_t slot = int32_t(i) * kStride + int32_t(j);
      const bool ok = m.kind == Get
          ? s.kit.Accessor(web, face.name, m.name, MemberNative, nullptr, slot)
          : s.kit.Method(web, face.name, m.name, MemberNative, m.length, slot);
      if (!ok) return false;
    }
  }
  return true;
}
}  // namespace ruyijs::web_gamepad
