#include "document_model/html_control.h"
#include "document_model/html_embedded_members.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
using namespace embedded;
bool EmbeddedString(JSContext* cx, JS::HandleValue value, DomString* text) {
  JS::RootedString converted(cx, JS::ToString(cx, value));
  if (!converted) {
    if (!JS_IsExceptionPending(cx) || JS_IsThrowingOutOfMemory(cx)) return false;
    JS_ClearPendingException(cx); text->clear(); return true;
  }
  JS::RootedValue string(cx, JS::StringValue(converted));
  return String(cx, string, text);
}
bool Read(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& m = kMembers[FunctionSlot(args)];
  auto* entry = ControlReceiver(cx, args.thisv(), m.iface);
  if (!entry) return false;
  auto& state = *entry->owner;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe(m.iface, m.name, int(ApiAccess::Read));
    if (m.kind == Dormant) return TypeError(cx, "Public embedded member has no initialized implementation");
    if (m.kind == Unsigned || m.kind == Loop) {
      double value = 0;
      if (!state.Check(state.model.EmbeddedInteger(entry->key, m.attr, &value))) return false;
      result.setNumber(value);
    } else {
      std::optional<DomString> attr;
      if (!state.Check(state.model.GetAttribute(entry->key, m.attr, &attr))) return false;
      if (m.kind == Boolean) result.setBoolean(attr.has_value());
      else if (!Value(cx, DomStringView(attr.value_or(u"")), &result)) return false;
    }
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
bool Write(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& m = kMembers[FunctionSlot(args)];
  auto* entry = ControlReceiver(cx, args.thisv(), m.iface);
  if (!entry) return false;
  if (m.kind == Dormant) return TypeError(cx, "Public embedded member has no initialized implementation");
  DomString text; int64_t number = 0;
  if (m.kind == Text && !ControlString(cx, args.get(0), &text)) return false;
  if (m.kind == Plain && !EmbeddedString(cx, args.get(0), &text)) return false;
  if (m.kind == Unsigned) {
    uint32_t value;
    if (!JS::ToUint32(cx, args.get(0), &value)) return false;
    number = value;
  } else if (m.kind == Loop) {
    int32_t value;
    if (!JS::ToInt32(cx, args.get(0), &value)) return false;
    number = value;
  }
  const bool present = JS::ToBoolean(args.get(0));
  auto& state = *entry->owner;
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe(m.iface, m.name, int(ApiAccess::Write));
    auto status = m.kind == Unsigned || m.kind == Loop ? state.model.SetEmbeddedInteger(entry->key, m.attr, number)
        : m.kind == Boolean && !present ? state.model.RemoveAttribute(entry->key, m.attr)
                                      : state.model.SetAttribute(entry->key, m.attr, text);
    if (!state.Check(status)) return false;
  }
  args.rval().setUndefined(); return true;
}
bool Method(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int kind = FunctionSlot(args);
  const char* iface = kind == 0 ? "HTMLEmbedElement" : "HTMLMarqueeElement";
  auto* entry = ControlReceiver(cx, args.thisv(), iface);
  if (!entry) return false;
  entry->owner->Observe(iface, kind == 0 ? "getSVGDocument" : kind == 1 ? "start" : "stop", int(ApiAccess::Call));
  if (kind == 0) return TypeError(cx, "Public embedded member has no initialized implementation");
  args.rval().setUndefined(); return true;
}
}
bool InstallEmbeddedElements(State& state) {
  for (size_t i = 0; i < std::size(embedded::kMembers); ++i) {
    const auto& m = embedded::kMembers[i];
    if (!state.Accessor(m.iface, m.name, Read, int32_t(i), m.set ? Write : nullptr)) return false;
  }
  const char* names[] = {"NONE", "LOADING", "LOADED", "ERROR"};
  for (int32_t i = 0; i < 4; ++i) {
    auto* iface = state.Find("HTMLTrackElement");
    for (bool constructor : {false, true}) {
      JS::RootedObject target(state.cx, constructor ? iface->constructor.get() : iface->prototype.get());
      if (!JS_DefineProperty(state.cx, target, names[i], i,
          JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)) return false;
    }
  }
  return state.Method("HTMLEmbedElement", "getSVGDocument", Method, 0, 0) &&
      state.Method("HTMLMarqueeElement", "start", Method, 0, 1) &&
      state.Method("HTMLMarqueeElement", "stop", Method, 0, 2);
}
}
