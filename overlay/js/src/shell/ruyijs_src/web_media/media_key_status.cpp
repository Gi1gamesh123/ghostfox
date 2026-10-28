#include "web_media/media_key_internal.h"
#include "web_media/media_runtime.h"
#include "runtime/page_helpers.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Symbol.h"

namespace ruyijs::web_media {
const JSClass kMediaKeyStatusMapClass = {"MediaKeyStatusMap", JSCLASS_HAS_RESERVED_SLOTS(StatusSlotCount)};

namespace {
bool Receiver(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleObject out) {
  return MediaReceiver(cx, args.thisv(), &kMediaKeyStatusMapClass, out);
}
bool Member(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); JS::RootedObject self(cx);
  if (!Receiver(cx, args, &self)) return false;
  switch (WebKit::Slot(args)) {
    case 0: args.rval().setBoolean(false); return true;
    case 1: args.rval().setUndefined(); return true;
    case 2: return EmptyIterator(cx, "entries", args.rval());
    case 3: return EmptyIterator(cx, "keys", args.rval());
    case 4: return EmptyIterator(cx, "values", args.rval());
    case 5: args.rval().setUndefined(); return true;
    default: args.rval().setUndefined(); return true;
  }
}
bool Size(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); JS::RootedObject self(cx);
  if (!Receiver(cx, args, &self)) return false; args.rval().setInt32(0); return true;
}
bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) {
  return WebKit::Reject(cx, argc, vp);
}
}

bool InstallMediaKeyStatusMap(State& state) {
  JS::RootedObject parent(state.cx, JS::GetRealmObjectPrototype(state.cx));
  if (!state.kit.Define("MediaKeyStatusMap", &kMediaKeyStatusMapClass, parent,
                        Illegal, &state.keyStatusMap)) return false;
  const char* methods[] = {"has", "get", "entries", "keys", "values", "forEach"};
  const unsigned lengths[] = {1, 1, 0, 0, 0, 1};
  for (unsigned i = 0; i < 6; ++i)
    if (!state.kit.Method(state.keyStatusMap, "MediaKeyStatusMap", methods[i], Member,
                          lengths[i], int32_t(i))) return false;
  if (!state.kit.Accessor(state.keyStatusMap, "MediaKeyStatusMap", "size", Size, nullptr, 0) ||
      !MediaConstructorLast(state, state.keyStatusMap)) return false;
  JS::RootedId iterator(state.cx, JS::PropertyKey::Symbol(
      JS::GetWellKnownSymbol(state.cx, JS::SymbolCode::iterator)));
  JS::RootedValue values(state.cx);
  if (!JS_GetProperty(state.cx, state.keyStatusMap.prototype, "values", &values) ||
      !values.isObject()) return false;
  return JS_DefinePropertyById(state.cx, state.keyStatusMap.prototype, iterator, values, 0);
}
}
