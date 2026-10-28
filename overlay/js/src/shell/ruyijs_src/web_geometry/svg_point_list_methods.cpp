#include "web_geometry/svg_points.h"
#include "js/Realm.h"
#include "js/Symbol.h"
#include "js/Wrapper.h"
namespace ruyijs::web_geometry {
namespace {
const char* names[] = {"length", "numberOfItems", "clear", "initialize", "getItem", "insertItemBefore",
                        "replaceItem", "removeItem", "appendItem"};
bool Argument(State& s, JS::HandleValue value, const char* member, JS::MutableHandleObject out) {
  if (!value.isObject()) {
    const auto error = std::string(member) + ": Argument 1 is not an object.";
    return WebKit::Fail(s.cx, error.c_str());
  }
  out.set(value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  if (out && JS::GetClass(out) == &kSvgPointClass && SvgPointData(out)) return true;
  const auto error = std::string(member) + ": Argument 1 does not implement interface SVGPoint.";
  return WebKit::Fail(s.cx, error.c_str());
}
}
bool SvgPointListMutate(State& s, JS::HandleObject list, unsigned operation, uint32_t index,
                        JS::HandleObject input, const char* member, JS::MutableHandleValue out) {
  if (js::GetProxyReservedSlot(list, 1).toBoolean())
    return SvgPointException(s, "NoModificationAllowedError", member, "Animated values cannot be set");
  uint32_t length;
  if (!SvgPointListLength(s.cx, list, &length)) return false;
  if ((operation == document_model::SvgPointsReplace || operation == document_model::SvgPointsRemove) && index >= length)
    return SvgPointException(s, "IndexSizeError", member, "Index out of range");
  JS::RootedValue removed(s.cx);
  if (operation == document_model::SvgPointsRemove && !SvgPointListItem(s.cx, list, index, &removed)) return false;
  SvgPointRef source = input ? SvgPointData(input)->value : nullptr, result;
  JS::RootedObject preparedOwner(s.cx), preparedCached(s.cx);
  // Prepare all cross-zone identity edges before committing an attached point.
  // An allocation failure must never leave attached=true with a null list edge.
  if (input && !source->attached) {
    {
      JSAutoRealm realm(s.cx, input); preparedOwner = list;
      if (!JS_WrapObject(s.cx, &preparedOwner)) return false;
    }
    {
      JSAutoRealm realm(s.cx, list); preparedCached = input;
      if (!JS_WrapObject(s.cx, &preparedCached)) return false;
      SvgPointListData(list)->cache.get().points[source.get()] = preparedCached;
    }
  }
  JS::RootedValue node(s.cx, js::GetProxyReservedSlot(list, 0));
  if (!JS_WrapValue(s.cx, &node) || !s.svgPorts.pointsMutate) return false;
  if (preparedOwner) SvgPointData(input)->list = preparedOwner;
  if (!s.svgPorts.pointsMutate(s.svgPorts.context, node, operation, index, source, &result)) return false;
  if (!result) { out.setUndefined(); return true; }
  if (operation == document_model::SvgPointsRemove) {
    JS::RootedObject object(s.cx, js::CheckedUnwrapStatic(&removed.toObject()));
    SvgPointData(object)->list = nullptr; out.set(removed); return true;
  }
  if (input && result == source) {
    // Insertion of detached points retains their original JS identity/realm.
    out.setObject(*input); return JS_WrapValue(s.cx, out);
  }
  if (operation == document_model::SvgPointsAppend) index = length;
  else if (operation == document_model::SvgPointsInitialize) index = 0;
  else index = std::min(index, length);
  return SvgPointListItem(s.cx, list, index, out);
}
bool SvgPointListMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); State& s = Self(args); unsigned slot = WebKit::Slot(args);
  const char* name = names[slot];
  JS::RootedObject list(cx, args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!list || JS::GetClass(list) != &kSvgPointListClass || !SvgPointListData(list)) {
    const auto error = std::string("'") + (slot < 2 ? "get " : "") + name +
        "' called on an object that does not implement interface SVGPointList.";
    return WebKit::Fail(cx, error.c_str());
  }
  const auto member = std::string("SVGPointList.") + name;
  if (slot < 2) { uint32_t length; if (!SvgPointListLength(cx, list, &length)) return false;
    args.rval().setNumber(length); return true; }
  const unsigned needed = slot == 2 ? 0 : (slot == 5 || slot == 6) ? 2 : 1;
  if (argc < needed) {
    const auto error = member + ": At least " + std::to_string(needed) + " argument" + (needed == 1 ? "" : "s") +
        " required, but only " + std::to_string(argc) + " passed";
    return WebKit::Fail(cx, error.c_str());
  }
  JS::RootedObject input(cx);
  if (slot == 3 || slot == 5 || slot == 6 || slot == 8)
    if (!Argument(s, args[0], member.c_str(), &input)) return false;
  uint32_t index = 0;
  if ((slot == 4 || slot == 7) && !JS::ToUint32(cx, args[0], &index)) return false;
  if ((slot == 5 || slot == 6) && !JS::ToUint32(cx, args[1], &index)) return false;
  if (slot == 4) {
    uint32_t length; if (!SvgPointListLength(cx, list, &length)) return false;
    if (index >= length) return SvgPointException(s, "IndexSizeError", member.c_str(), "Index out of range");
    return SvgPointListItem(cx, list, index, args.rval());
  }
  static const unsigned operations[] = {0, 0, document_model::SvgPointsClear, document_model::SvgPointsInitialize,
      0, document_model::SvgPointsInsert, document_model::SvgPointsReplace, document_model::SvgPointsRemove,
      document_model::SvgPointsAppend};
  return SvgPointListMutate(s, list, operations[slot], index, input, member.c_str(), args.rval());
}
bool SvgPointIndexedSet(JSContext* cx, JS::HandleObject list, uint32_t index, JS::HandleValue value) {
  State& s = *SvgPointListData(list)->state; JS::RootedObject input(cx); JS::RootedValue out(cx);
  if (!value.isObject())
    return WebKit::Fail(cx, "SVGPointList indexed setter: Value being assigned to SVGPointList setter is not an object.");
  input = js::CheckedUnwrapStatic(&value.toObject());
  if (!input || JS::GetClass(input) != &kSvgPointClass || !SvgPointData(input))
    return WebKit::Fail(cx, "SVGPointList indexed setter: Value being assigned to SVGPointList setter does not implement interface SVGPoint.");
  return SvgPointListMutate(s, list, document_model::SvgPointsReplace, index, input, "SVGPointList indexed setter", &out);
}
namespace {
// Symbol keys keep definition order: @@toStringTag (from Define) moves after @@iterator.
bool RestoreToStringTag(State& s) {
  JS::RootedObject proto(s.cx, s.svgPointList.prototype);
  JS::RootedId tag(s.cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(s.cx, JS::SymbolCode::toStringTag)));
  JS::RootedString name(s.cx, JS_NewStringCopyZ(s.cx, "SVGPointList"));
  JS::ObjectOpResult deleted;
  return name && JS_DeletePropertyById(s.cx, proto, tag, deleted) &&
         JS_DefinePropertyById(s.cx, proto, tag, name, JSPROP_READONLY);
}
}
bool InstallSvgPointList(State& s) {
  JS::RootedObject plain(s.cx, JS::GetRealmObjectPrototype(s.cx));
  if (!s.kit.Define("SVGPointList", &kSvgPointListClass, plain, WebKit::Reject, &s.svgPointList) ||
      !JS_DeleteProperty(s.cx, s.svgPointList.prototype, "constructor")) return false;
  for (unsigned k = 0; k < 9; ++k) {  // Firefox order: the methods, then length/numberOfItems.
    const unsigned i = (k + 2) % 9;
    if (i < 2) { if (!s.kit.Accessor(s.svgPointList, "SVGPointList", names[i], SvgPointListMember, nullptr, i)) return false; }
    else if (!s.kit.Method(s.svgPointList, "SVGPointList", names[i], SvgPointListMember,
        i == 2 ? 0 : (i == 5 || i == 6) ? 2 : 1, i)) return false;
  }
  JS::RootedObject array(s.cx, JS::GetRealmArrayPrototype(s.cx)); JS::RootedValue values(s.cx);
  JS::RootedId id(s.cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(s.cx, JS::SymbolCode::iterator)));
  return JS_DefineProperty(s.cx, s.svgPointList.prototype, "constructor", s.svgPointList.constructor, 0) &&
      array && JS_GetProperty(s.cx, array, "values", &values) &&
      JS_DefinePropertyById(s.cx, s.svgPointList.prototype, id, values, 0) && RestoreToStringTag(s);
}
}
