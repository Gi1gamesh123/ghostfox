#include "web_graphics/internal.h"
#include "js/Realm.h"

namespace ruyijs::web_graphics {
namespace {
const JSClass kPrecisionClass = {"WebGLShaderPrecisionFormat", JSCLASS_HAS_RESERVED_SLOTS(3)};
const JSClass kActiveClass = {"WebGLActiveInfo", JSCLASS_HAS_RESERVED_SLOTS(3)};
bool ResultGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int field = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!WebKit::Check(cx, args.thisv(), field < 3 ? &kPrecisionClass : &kActiveClass,
                    &self, "Illegal invocation")) return false;
  args.rval().set(JS::GetReservedSlot(self, field % 3));
  return true;
}
}
bool GetShaderPrecision(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *detail::Self(args);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  int32_t shader, type;
  if (!detail::ToEnum(cx, args.get(0), &shader) || !detail::ToEnum(cx, args.get(1), &type)) return false;
  if ((shader != kVertexShader && shader != kFragmentShader) || type < 0x8DF0 || type > 0x8DF5) {
    detail::SetError(*detail::ContextOf(self), kInvalidEnum);
    args.rval().setNull(); return true;
  }
  JS::RootedObject result(cx, s.kit.New(&kPrecisionClass, s.precision.prototype));
  if (!result) return false;
  const bool integer = type >= 0x8DF3;
  JS::SetReservedSlot(result, 0, JS::Int32Value(integer ? 31 : 127));
  JS::SetReservedSlot(result, 1, JS::Int32Value(integer ? 30 : 127));
  JS::SetReservedSlot(result, 2, JS::Int32Value(integer ? 0 : 23));
  args.rval().setObject(*result); return true;
}
bool InstallResultInterfaces(State& s) {
  JS::RootedObject parent(s.cx, JS::GetRealmObjectPrototype(s.cx));
  if (!s.kit.Define("WebGLShaderPrecisionFormat", &kPrecisionClass, parent, WebKit::Reject, &s.precision) ||
      !s.kit.Define("WebGLActiveInfo", &kActiveClass, parent, WebKit::Reject, &s.activeInfo)) return false;
  const char* precision[] = {"rangeMin","rangeMax","precision"};
  const char* active[] = {"size","type","name"};
  for (int i = 0; i < 3; ++i)
    if (!s.kit.Accessor(s.precision, "WebGLShaderPrecisionFormat", precision[i], ResultGet, nullptr, i) ||
        !s.kit.Accessor(s.activeInfo, "WebGLActiveInfo", active[i], ResultGet, nullptr, i+3)) return false;
  return true;
}
}
