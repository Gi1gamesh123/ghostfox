// HTMLMetaElement DOMString reflection. The Element's domain attribute map is
// the only state; parser, setAttribute, clone and property writes share it.
#include "document_model/state.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <cstring>

namespace ruyijs::document_model {
namespace {
struct Property { const char* name; const char16_t* attribute; };
constexpr Property kProperties[] = {
    {"name", u"name"}, {"httpEquiv", u"http-equiv"}, {"content", u"content"},
    {"media", u"media"}, {"scheme", u"scheme"}};

Wrapper* MetaReceiver(JSContext* cx, const JS::CallArgs& args) {
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  Wrapper* entry = object && JS::GetClass(object) == &kNodeClass
      ? static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate()) : nullptr;
  if (!entry || entry->dead || !entry->owner) {
    TypeError(cx, "HTMLMetaElement member called on incompatible receiver");
    return nullptr;
  }
  State& state = *entry->owner;
  JSAutoRealm realm(cx, state.page);
  NodeSnapshot value;
  if (!state.Check(state.model.Inspect(entry->key, &value))) return nullptr;
  if (std::strcmp(value.interface_name, "HTMLMetaElement")) {
    TypeError(state.cx, "HTMLMetaElement member called on incompatible receiver");
    return nullptr;
  }
  return entry;
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = MetaReceiver(cx, args);
  if (!entry) return false;
  State& state = *entry->owner;
  const auto& property = kProperties[FunctionSlot(args)];
  std::optional<DomString> value;
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe("HTMLMetaElement", property.name, int(ApiAccess::Read));
    if (!state.Check(state.model.GetAttribute(entry->key, property.attribute, &value)))
      return false;
  }
  return Value(cx, value ? DomStringView(*value) : DomStringView(), args.rval());
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = MetaReceiver(cx, args);
  if (!entry) return false;
  State& state = *entry->owner;
  const auto& property = kProperties[FunctionSlot(args)];
  DomString value;
  if (!String(cx, args.get(0), &value)) return false;
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe("HTMLMetaElement", property.name, int(ApiAccess::Write));
    if (!state.Check(state.model.SetAttribute(entry->key, property.attribute, value)))
      return false;
  }
  args.rval().setUndefined();
  return true;
}
}
bool InstallMetaElement(State& state) {
  for (size_t index = 0; index < std::size(kProperties); ++index)
    if (!state.Accessor("HTMLMetaElement", kProperties[index].name,
                        Get, int32_t(index), Set)) return false;
  return true;
}
}
