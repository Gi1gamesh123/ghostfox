// The URLSearchParams input converter and the URL-facing factory. A sequence of
// pairs is read through the iterator protocol and a record through its own
// enumerable string keys, so the constructor accepts exactly what the donor
// does; NewSearchParams creates the live object a URL keeps for its query.
#include "web_platform/state.h"

#include "runtime/web_kit.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/ForOfIterator.h"
#include "js/Id.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::web_platform {
using Entry = std::pair<std::string, std::string>;

bool IsSearchParams(JS::HandleValue value) {
  return value.isObject() && JS::GetClass(&value.toObject()) == &kSearchParamsClass;
}

namespace {
bool PairsFromIterable(JSContext* cx, JS::HandleValue init, std::vector<Entry>* entries) {
  JS::ForOfIterator iterate(cx);
  if (!iterate.init(init, JS::ForOfIterator::ThrowOnNonIterable)) return false;
  JS::RootedValue item(cx);
  while (true) {
    bool done = false;
    if (!iterate.next(&item, &done)) return false;
    if (done) break;
    if (!item.isObject())
      return WebKit::Fail(cx, "URLSearchParams requires a sequence of pairs");
    JS::RootedObject pair(cx, &item.toObject());
    JS::RootedValue key(cx), value(cx);
    std::string name, text;
    if (!JS_GetElement(cx, pair, 0, &key) || !JS_GetElement(cx, pair, 1, &value) ||
        !Convert(cx, key, &name) || !Convert(cx, value, &text))
      return false;
    entries->emplace_back(std::move(name), std::move(text));
  }
  return true;
}

bool RecordFromObject(JSContext* cx, JS::HandleObject object,
                      std::vector<Entry>* entries) {
  JS::Rooted<JS::IdVector> ids(cx, JS::IdVector(cx));
  if (!JS_Enumerate(cx, object, &ids)) return false;
  for (size_t index = 0; index < ids.length(); ++index) {
    JS::RootedId id(cx, ids[index]);
    JS::RootedValue key(cx), value(cx);
    std::string name, text;
    if (!JS_IdToValue(cx, id, &key) || !JS_GetPropertyById(cx, object, id, &value) ||
        !Convert(cx, key, &name) || !Convert(cx, value, &text))
      return false;
    entries->emplace_back(std::move(name), std::move(text));
  }
  return true;
}
}  // namespace

bool ConvertInit(JSContext* cx, JS::HandleValue init, std::vector<Entry>* entries) {
  JS::RootedObject object(cx, &init.toObject());
  JS::RootedId iteratorId(
      cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::iterator)));
  bool iterable = false;
  if (!JS_HasPropertyById(cx, object, iteratorId, &iterable)) return false;
  return iterable ? PairsFromIterable(cx, init, entries)
                  : RecordFromObject(cx, object, entries);
}

JSObject* NewSearchParams(State& state, const std::string& query, JS::HandleObject owner) {
  JSContext* cx = state.cx;
  JS::RootedObject object(cx, state.kit.New(&kSearchParamsClass, state.params.prototype));
  JS::RootedValue text(cx);
  if (!object || !Value(cx, query, &text)) return nullptr;
  JS::SetReservedSlot(object, kParamsQuerySlot, text);
  JS::SetReservedSlot(object, kParamsOwnerSlot, JS::ObjectValue(*owner));
  return object;
}
}  // namespace ruyijs::web_platform