#include "document_model/html_form.h"
#include "js/Object.h"
#include "js/PropertyDescriptor.h"
#include <set>

namespace ruyijs::document_model {
namespace {
bool ClearGenerated(State& state, JS::HandleObject target, JS::HandleObject cache) {
  if (!cache) return true;
  JS::Rooted<JS::IdVector> ids(state.cx, JS::IdVector(state.cx));
  if (!JS_Enumerate(state.cx, cache, &ids)) return false;
  for (size_t i = 0; i < ids.length(); ++i) {
    JS::RootedId id(state.cx, ids[i]);
    JS::RootedValue old(state.cx);
    JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> descriptor(state.cx);
    if (!JS_GetPropertyById(state.cx, cache, id, &old) ||
        !JS_GetOwnPropertyDescriptorById(state.cx, target, id, &descriptor)) return false;
    // Only retire our own unchanged data descriptor. A user's setter, value,
    // configurability or enumerability override stays in control.
    if (descriptor.isNothing() || !descriptor->isDataDescriptor() ||
        descriptor->value() != old || !descriptor->configurable() ||
        descriptor->enumerable() != id.isInt() || descriptor->writable() != id.isInt()) continue;
    JS::ObjectOpResult removed;
    if (!JS_DeletePropertyById(state.cx, target, id, removed) || !removed.ok()) return false;
  }
  return true;
}
bool Put(State& state, JS::HandleObject target, JS::HandleObject cache, JS::HandleId id,
         JS::HandleValue value, bool inherited) {
  bool exists = false;
  if (!(inherited ? JS_HasPropertyById(state.cx, target, id, &exists)
                  : JS_HasOwnPropertyById(state.cx, target, id, &exists))) return false;
  if (exists) return true;
  return JS_DefinePropertyById(state.cx, target, id, value, id.isInt() ? JSPROP_ENUMERATE : JSPROP_READONLY) &&
      JS_DefinePropertyById(state.cx, cache, id, value, JSPROP_ENUMERATE);
}
}
bool MaterializeForm(State& state, JS::HandleObject collection, CollectionRecord& record) {
  JS::RootedValue scope(state.cx);
  if (!state.Wrap(record.scope, &scope)) return false;
  JS::RootedObject form(state.cx, &scope.toObject());
  JS::SetReservedSlot(collection, 2, scope);
  JS::RootedObject oldForm(state.cx, JS::GetReservedSlot(collection, 3).isObject()
      ? &JS::GetReservedSlot(collection, 3).toObject() : nullptr);
  JS::RootedObject oldNames(state.cx, JS::GetReservedSlot(collection, 4).isObject()
      ? &JS::GetReservedSlot(collection, 4).toObject() : nullptr);
  if (!ClearGenerated(state, form, oldForm) || !ClearGenerated(state, collection, oldNames)) return false;
  JS::RootedObject nullProto(state.cx);
  JS::RootedObject formCache(state.cx, JS_NewObjectWithGivenProto(state.cx, nullptr, nullProto));
  JS::RootedObject namedCache(state.cx, JS_NewObjectWithGivenProto(state.cx, nullptr, nullProto));
  if (!formCache || !namedCache) return false;
  JS::SetReservedSlot(collection, 3, JS::ObjectValue(*formCache));
  JS::SetReservedSlot(collection, 4, JS::ObjectValue(*namedCache));
  std::set<DomString> seen;
  std::vector<DomString> names;
  for (size_t i = 0; i < record.items.size(); ++i) {
    JS::RootedId id(state.cx, JS::PropertyKey::Int(int32_t(i)));
    JS::RootedValue node(state.cx);
    if (!state.Wrap(record.items[i], &node) || !Put(state, form, formCache, id, node, false)) return false;
    for (const char16_t* attr : {u"id", u"name"}) {
      std::optional<DomString> value;
      if (!state.Check(state.model.GetAttribute(record.items[i], attr, &value))) return false;
      if (value && !value->empty() && seen.insert(*value).second) names.push_back(*value);
    }
  }
  for (const auto& name : names) {
    JS::RootedValue key(state.cx), value(state.cx);
    JS::RootedId id(state.cx);
    if (!Value(state.cx, DomStringView(name), &key) || !JS_ValueToId(state.cx, key, &id)) return false;
    if (id.isInt()) continue;
    if (!FormNamed(state, record.scope, name, &value) ||
        !Put(state, form, formCache, id, value, false) ||
        !Put(state, collection, namedCache, id, value, true)) return false;
  }
  return true;
}
}
