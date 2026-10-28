// NamedNodeMap: the live attributes collection of one element. The Attr objects
// it hands out are owned by attr_object.cpp; this file only maps the collection
// onto the element's attribute list. Owner: document_model.
#include "document_model/collections.h"
#include "document_model/state.h"
#include <set>
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/PropertyDescriptor.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
enum AttrOperation { GetNamed, SetNamed, RemoveNamed, AtIndex };
enum NamespaceOperation { GetNS, SetNS, RemoveNS };

bool LookupNS(State& state, CollectionRecord& record, NamespaceOperation op,
              DomStringView namespace_uri, DomStringView local, JS::MutableHandleValue result) {
  // Conversion can change the element's live attributes. Take the current owner
  // snapshot after conversion and never retain a view across Check/SyncCollections.
  std::vector<AttributeSnapshot> values;
  if (!state.Check(state.model.Attributes(record.scope, &values))) return false;
  for (const auto& value : values) {
    if (value.namespace_uri != namespace_uri || value.local_name != local) continue;
    if (op == GetNS)
      return AttributeValue(state, record.scope,
          AttributeArgument(value.namespace_uri, value.local_name, value.name), result);
    return DetachedAttributeFor(state, value, result) &&
        state.Check(state.model.RemoveAttributeNS(record.scope, namespace_uri, local));
  }
  if (op == GetNS) { result.setNull(); return true; }
  return ThrowDOM(state, "NotFoundError", "No attribute with that namespace and name");
}

bool NamespaceCall(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* record = CollectionReceiver(cx, args.thisv(), CollectionKind::AttributeMap);
  if (!record) return false;
  const auto op = NamespaceOperation(FunctionSlot(args));
  if (argc < (op == SetNS ? 1u : 2u))
    return TypeError(cx, "NamedNodeMap method requires arguments");
  DomString namespace_uri, local;
  if (op != SetNS && (!String(cx, args[0], &namespace_uri, true) || !String(cx, args[1], &local))) return false;
  JS::RootedValue attribute(cx, args[0]), result(cx);
  {
    State& state = *record->dataOwner;
    JSAutoRealm realm(cx, state.page);
    if (op == SetNS) {
      if (!JS_WrapValue(cx, &attribute) || !SetAttributeNode(state, record->scope, attribute, &result)) return false;
    } else if (!LookupNS(state, *record, op, namespace_uri, local, &result)) return false;
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}

bool Lookup(State& state, CollectionRecord& record, AttrOperation op, DomStringView name,
            JS::HandleValue value, JS::MutableHandleValue result) {
  if (op == SetNamed) {
    AttributeState attribute;
    if (!AttrStateOf(state, value, &attribute))
      return TypeError(state.cx, "setNamedItem requires an Attr of this realm");
    if (!attribute.detached && (attribute.dataOwner != &state || !Same(attribute.owner, record.scope)))
      return ThrowDOM(state, "InUseAttributeError", "The attribute belongs to another element");
    Status status = attribute.namespace_uri.empty()
        ? state.model.SetAttribute(record.scope, attribute.name, attribute.value)
        : state.model.SetAttributeNS(record.scope, attribute.namespace_uri,
                                     attribute.name, attribute.value);
    if (!state.Check(status)) return false;
    result.setNull();
    return true;
  }
  // Check may refresh the live record: never retain its string storage across it.
  const auto entries = record.strings;
  for (const DomString& entry : entries) {
    DomStringView namespace_uri, local_name, qualified;
    SplitAttributeArgument(entry, &namespace_uri, &local_name, &qualified);
    if (qualified != name) continue;
    if (op == GetNamed) return AttributeValue(state, record.scope, entry, result);
    if (!AttributeValue(state, record.scope, entry, result)) return false;
    Status status = namespace_uri.empty()
        ? state.model.RemoveAttribute(record.scope, name)
        : state.model.RemoveAttributeNS(record.scope, namespace_uri, local_name);
    if (!state.Check(status)) return false;
    return true;
  }
  if (op == GetNamed) { result.setNull(); return true; }
  return ThrowDOM(state, "NotFoundError", "No attribute with that name");
}

bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* record = CollectionReceiver(cx, args.thisv(), CollectionKind::AttributeMap);
  if (!record) return false;
  const auto op = AttrOperation(FunctionSlot(args));
  if (op == AtIndex) {
    uint32_t index = 0;
    if (!JS::ToUint32(cx, args.get(0), &index)) return false;
    if (index >= CollectionCount(*record)) { args.rval().setNull(); return true; }
    return CollectionIndex(*record->dataOwner, *record, index, args.rval());
  }
  DomString name;
  if (op != SetNamed && !String(cx, args.get(0), &name)) return false;
  JS::RootedValue attribute(cx, args.get(0)), result(cx);
  {
    State& state = *record->dataOwner;
    JSAutoRealm realm(cx, state.page);
    if (!JS_WrapValue(cx, &attribute) || !Lookup(state, *record, op, name, attribute, &result)) return false;
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
// Reserved slot holding the null-prototype cache of named properties this
// owner defined at the last materialization.
constexpr unsigned kNamedCacheSlot = 4;

// Retires only our own unchanged named descriptors; script overrides stay.
bool ClearNames(JSContext* cx, JS::HandleObject map, JS::HandleObject cache) {
  if (!cache) return true;
  JS::Rooted<JS::IdVector> ids(cx, JS::IdVector(cx));
  if (!JS_Enumerate(cx, cache, &ids)) return false;
  JS::RootedId id(cx);
  JS::RootedValue old(cx);
  JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> own(cx);
  for (size_t i = 0; i < ids.length(); ++i) {
    id = ids[i];
    if (!JS_GetPropertyById(cx, cache, id, &old) ||
        !JS_GetOwnPropertyDescriptorById(cx, map, id, &own)) return false;
    if (own.isNothing() || !own->isDataDescriptor() || own->value() != old ||
        !own->configurable() || own->enumerable() || own->writable()) continue;
    JS::ObjectOpResult removed;
    if (!JS_DeletePropertyById(cx, map, id, removed) || !removed.ok()) return false;
  }
  return true;
}

bool HasAsciiUpper(DomStringView name) {
  for (char16_t c : name) if (c >= u'A' && c <= u'Z') return true;
  return false;
}
}  // namespace

// WebIDL [LegacyUnenumerableNamedProperties] getter of NamedNodeMap: the
// supported names are the attributes' qualified names in order, without
// duplicates, and without names containing ASCII upper alphas for an HTML
// element in an HTML document. A name is visible only when it is not an index
// and the prototype chain does not define it (no [LegacyOverrideBuiltIns]).
// Each value is the very Attr object the attribute's index property holds.
bool MaterializeAttributeNames(State& state, JS::HandleObject map, CollectionRecord& record) {
  JSContext* cx = state.cx;
  const JS::Value slot = JS::GetReservedSlot(map, kNamedCacheSlot);
  JS::RootedObject oldCache(cx, slot.isObject() ? &slot.toObject() : nullptr);
  if (!ClearNames(cx, map, oldCache)) return false;
  JS::RootedObject nullProto(cx);
  JS::RootedObject cache(cx, JS_NewObjectWithGivenProto(cx, nullptr, nullProto));
  if (!cache) return false;
  JS::SetReservedSlot(map, kNamedCacheSlot, JS::ObjectValue(*cache));
  State& owner = *record.dataOwner;
  NodeSnapshot element;
  if (!owner.Check(owner.model.Inspect(record.scope, &element))) return false;
  const bool html = element.is_html_element && element.document_mode == DocumentMode::Html;
  JS::RootedObject prototype(cx);
  if (!JS_GetPrototype(cx, map, &prototype)) return false;
  std::set<DomString> seen;
  JS::RootedValue key(cx), value(cx);
  JS::RootedId id(cx);
  // Index i of the record is the attribute whose Attr index property is i.
  const std::vector<DomString> entries = record.strings;
  for (size_t index = 0; index < entries.size(); ++index) {
    DomStringView namespace_uri, local, qualified;
    SplitAttributeArgument(entries[index], &namespace_uri, &local, &qualified);
    if ((html && HasAsciiUpper(qualified)) || !seen.emplace(qualified).second) continue;
    if (!Value(cx, qualified, &key) || !JS_ValueToId(cx, key, &id)) return false;
    if (id.isInt()) continue;
    bool found = false;
    if (prototype && !JS_HasPropertyById(cx, prototype, id, &found)) return false;
    if (found) continue;
    if (!JS_HasOwnPropertyById(cx, map, id, &found)) return false;
    if (found) continue;
    // Reuse the materialized index property so attributes[name] === attributes[i].
    if (!JS_GetElement(cx, map, uint32_t(index), &value) ||
        !JS_DefinePropertyById(cx, map, id, value, JSPROP_READONLY) ||
        !JS_DefinePropertyById(cx, cache, id, value, JSPROP_ENUMERATE)) return false;
  }
  return true;
}

bool InstallAttributeMap(State& state) {
  return state.Accessor("NamedNodeMap", "length", CollectionLength, int32_t(CollectionKind::AttributeMap)) &&
      state.Method("NamedNodeMap", "getNamedItem", Call, 1, GetNamed) &&
      state.Method("NamedNodeMap", "setNamedItem", Call, 1, SetNamed) &&
      state.Method("NamedNodeMap", "removeNamedItem", Call, 1, RemoveNamed) &&
      state.Method("NamedNodeMap", "item", Call, 1, AtIndex) &&
      state.Method("NamedNodeMap", "getNamedItemNS", NamespaceCall, 2, GetNS) &&
      state.Method("NamedNodeMap", "setNamedItemNS", NamespaceCall, 1, SetNS) &&
      state.Method("NamedNodeMap", "removeNamedItemNS", NamespaceCall, 2, RemoveNS) &&
      InstallCollectionIterator(state, "NamedNodeMap", int32_t(CollectionIteration::Values));
}
}  // namespace ruyijs::document_model
