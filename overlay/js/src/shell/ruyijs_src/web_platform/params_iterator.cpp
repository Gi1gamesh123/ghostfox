// The URLSearchParams iterator. It holds the parameter object and a position;
// every step re-reads the one encoded query, so a mutation between steps is
// observed the way the specification requires.
#include "web_platform/state.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::web_platform {
namespace {
enum IteratorSlot { kIteratorParams = 0, kIteratorIndex, kIteratorKind, kIteratorSlots };
enum IteratorKind { kEntries = 0, kKeys, kValues };
}  // namespace

const JSClass kParamsIteratorClass = {
    "URLSearchParams Iterator", JSCLASS_HAS_RESERVED_SLOTS(kIteratorSlots)};

bool ParamsIterator(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  if (!state.kit.Check(cx, args.thisv(), &kSearchParamsClass, &self,
                       "URLSearchParams member called on an incompatible receiver"))
    return false;
  JS::RootedObject object(
      cx, state.kit.New(&kParamsIteratorClass, state.paramsIteratorPrototype));
  if (!object) return false;
  JS::SetReservedSlot(object, kIteratorParams, JS::ObjectValue(*self));
  JS::SetReservedSlot(object, kIteratorIndex, JS::Int32Value(0));
  JS::SetReservedSlot(object, kIteratorKind, JS::Int32Value(WebKit::Slot(args)));
  args.rval().setObject(*object);
  return true;
}

// The iterator is itself iterable: %URLSearchParamsIteratorPrototype%
// [@@iterator] returns the receiver so for-of and Array.from work on it.
bool ParamsIdentity(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!args.thisv().isObject() ||
      JS::GetClass(&args.thisv().toObject()) != &kParamsIteratorClass)
    return WebKit::Fail(cx, "URLSearchParams iterator called on an incompatible receiver");
  args.rval().set(args.thisv());
  return true;
}

bool ParamsNext(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!args.thisv().isObject() ||
      JS::GetClass(&args.thisv().toObject()) != &kParamsIteratorClass)
    return WebKit::Fail(cx, "URLSearchParams iterator called on an incompatible receiver");
  JS::RootedObject iterator(cx, &args.thisv().toObject());
  JS::RootedValue owner(cx, JS::GetReservedSlot(iterator, kIteratorParams));
  if (!owner.isObject())
    return WebKit::Fail(cx, "URLSearchParams iterator is not initialized");
  JS::RootedObject params(cx, &owner.toObject());
  std::vector<std::pair<std::string, std::string>> entries;
  if (!ParamsEntries(cx, params, &entries)) return false;
  const size_t index = size_t(JS::GetReservedSlot(iterator, kIteratorIndex).toInt32());
  const IteratorKind kind = IteratorKind(JS::GetReservedSlot(iterator, kIteratorKind).toInt32());
  JS::RootedObject result(cx, JS_NewPlainObject(cx));
  JS::RootedValue value(cx), done(cx, JS::BooleanValue(index >= entries.size()));
  if (!result) return false;
  if (index < entries.size()) {
    const auto& entry = entries[index];
    if (kind == kEntries) {
      JS::RootedObject pair(cx, JS::NewArrayObject(cx, 2));
      JS::RootedValue first(cx), second(cx);
      if (!pair || !Value(cx, entry.first, &first) || !Value(cx, entry.second, &second) ||
          !JS_SetElement(cx, pair, 0, first) || !JS_SetElement(cx, pair, 1, second))
        return false;
      value.setObject(*pair);
    } else if (!Value(cx, kind == kKeys ? entry.first : entry.second, &value)) return false;
    JS::SetReservedSlot(iterator, kIteratorIndex, JS::Int32Value(int32_t(index + 1)));
  }
  if (!JS_DefineProperty(cx, result, "done", done, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, result, "value", value, JSPROP_ENUMERATE)) return false;
  args.rval().setObject(*result);
  return true;
}
}  // namespace ruyijs::web_platform
