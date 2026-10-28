// Highlight: setlike<AbstractRange> with priority (long) and type (HighlightType).
// Slots: 0 the backing Set, 1 priority, 2 type index. The shared collection member
// body (CollectionMember) also serves HighlightRegistry (highlight_registry.cpp).
#include "web_styling/highlight.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include "js/MapAndSet.h"
#include "js/Wrapper.h"
#include "web_styling/internal.h"
#include <iterator>
#include <string>

namespace ruyijs::web_styling {
namespace {
const JSClass kHighlightClass = {"Highlight", JSCLASS_HAS_RESERVED_SLOTS(3)};
enum Op { Add, Clear, Delete, Entries, Keys, Values, ForEach, Has, Priority, Type, Size };
const char* const kNames[] = {"add", "clear", "delete", "entries", "keys", "values", "forEach", "has"};
const unsigned kLengths[] = {1, 0, 1, 0, 0, 0, 1, 1};
const char* const kTypes[] = {"highlight", "spelling-error", "grammar-error"};

bool Fail(JSContext* cx, const std::string& text) { return WebKit::Fail(cx, text.c_str()); }
// An AbstractRange argument: Firefox's "is not an object" / interface texts.
bool RangeArgument(JSContext* cx, State& s, const std::string& where, unsigned index, JS::HandleValue value) {
  const std::string prefix = where + ": Argument " + std::to_string(index + 1);
  if (!value.isObject()) return Fail(cx, prefix + " is not an object.");
  if (!s.rangeBrand || !s.rangeBrand(value)) return Fail(cx, prefix + " does not implement interface AbstractRange.");
  return true;
}
bool Construct(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *detail::Self(args);
  if (!args.isConstructing()) return Fail(cx, "Highlight constructor: 'new' is required");
  // Firefox's variadic conversion reports every element as "Argument 1".
  for (unsigned i = 0; i < argc; ++i)
    if (!RangeArgument(cx, s, "Highlight constructor", 0, args[i])) return false;
  JS::RootedObject set(cx, JS::NewSetObject(cx));
  JS::RootedObject self(cx, set ? JS_NewObjectForConstructor(cx, &kHighlightClass, args) : nullptr);
  if (!self) return false;
  JS::SetReservedSlot(self, 0, JS::ObjectValue(*set));
  JS::SetReservedSlot(self, 1, JS::Int32Value(0));
  JS::SetReservedSlot(self, 2, JS::Int32Value(0));
  for (unsigned i = 0; i < argc; ++i)
    if (!JS::SetAdd(cx, set, args[i])) return false;
  args.rval().setObject(*self);
  return true;
}
bool Member(JSContext* cx, unsigned argc, JS::Value* vp) {
  const int op = WebKit::Slot(JS::CallArgsFromVp(argc, vp));
  return CollectionMember(cx, argc, vp, &kHighlightClass, "Highlight", op == Size ? "size" : kNames[op]);
}
// priority/type: one native for getter and setter, told apart by the callee's
// "get "/"set " name prefix that WebKit::Accessor gives them.
bool Attribute(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = WebKit::Slot(args);
  JSObject* raw = args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr;
  if (!raw || JS::GetClass(raw) != &kHighlightClass) return WebKit::FailBrand(cx, args, "Highlight");
  JS::RootedObject self(cx, raw);
  const bool type = slot == Type;
  std::string label;
  if (!WebKit::BrandText(cx, args, "Highlight", &label)) return false;
  if (label.find("'get ") == 0) {
    const int32_t value = JS::GetReservedSlot(self, type ? 2 : 1).toInt32();
    if (!type) { args.rval().setInt32(value); return true; }
    JS::RootedString text(cx, JS_NewStringCopyZ(cx, kTypes[value]));
    if (!text) return false;
    args.rval().setString(text);
    return true;
  }
  args.rval().setUndefined();
  if (!type) {
    int32_t value = 0;
    if (!JS::ToInt32(cx, args.get(0), &value)) return false;
    JS::SetReservedSlot(self, 1, JS::Int32Value(value));
    return true;
  }
  std::string text;
  if (!detail::ToAscii(cx, args.get(0), &text)) return false;
  // An enumeration attribute ignores a value outside the enumeration.
  for (int32_t i = 0; i < int32_t(std::size(kTypes)); ++i)
    if (text == kTypes[i]) JS::SetReservedSlot(self, 2, JS::Int32Value(i));
  return true;
}
}  // namespace

bool CollectionMember(JSContext* cx, unsigned argc, JS::Value* vp, const JSClass* clasp, const char* iface,
                      const char* member) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *detail::Self(args);
  const bool map = clasp != &kHighlightClass;
  JSObject* raw = args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr;
  if (!raw || JS::GetClass(raw) != clasp) return WebKit::FailBrand(cx, args, iface);
  JS::RootedObject self(cx, raw), store(cx, &JS::GetReservedSlot(raw, 0).toObject());
  const std::string name = member;
  if (name == "size") {
    args.rval().setNumber(double(map ? JS::MapSize(cx, store) : JS::SetSize(cx, store)));
    return true;
  }
  const std::string where = std::string(iface) + "." + name;
  const unsigned required = name == "set" || name == "highlightsFromPoint" ? 2 :
      name == "clear" || name == "entries" || name == "keys" || name == "values" ? 0 : 1;
  if (argc < required)
    return Fail(cx, where + ": At least " + std::to_string(required) + " argument" + (required == 1 ? "" : "s") +
                    " required, but only " + std::to_string(argc) + " passed");
  if (name == "entries") return map ? JS::MapEntries(cx, store, args.rval()) : JS::SetEntries(cx, store, args.rval());
  if (name == "keys") return map ? JS::MapKeys(cx, store, args.rval()) : JS::SetValues(cx, store, args.rval());
  if (name == "values") return map ? JS::MapValues(cx, store, args.rval()) : JS::SetValues(cx, store, args.rval());
  if (name == "clear") { args.rval().setUndefined(); return map ? JS::MapClear(cx, store) : JS::SetClear(cx, store); }
  if (name == "highlightsFromPoint") {
    // No highlight is painted, so no point hits one; the coordinates still convert.
    for (unsigned i = 0; i < 2; ++i) {
      double value = 0;
      if (!JS::ToNumber(cx, args[i], &value)) return false;
      if (!std::isfinite(float(value)))
        return Fail(cx, where + ": Argument " + std::to_string(i + 1) + " is not a finite floating-point value.");
    }
    JSObject* none = JS::NewArrayObject(cx, 0);
    if (!none) return false;
    args.rval().setObject(*none);
    return true;
  }
  if (name == "forEach") {
    if (!args[0].isObject()) return Fail(cx, where + ": Argument 1 is not an object.");
    if (!JS::IsCallable(&args[0].toObject())) return Fail(cx, where + ": Argument 1 is not callable.");
    JS::RootedValue entries(cx), entry(cx), ignored(cx), callback(cx, args[0]);
    JS::ForOfIterator it(cx);
    if (!(map ? JS::MapEntries(cx, store, &entries) : JS::SetValues(cx, store, &entries)) || !it.init(entries)) return false;
    JS::RootedValueArray<3> call(cx);
    call[2].setObject(*self);  // the interface object, never the backing collection
    for (bool done = false;;) {
      if (!it.next(&entry, &done)) return false;
      if (done) break;
      if (map) {
        JS::RootedObject pair(cx, &entry.toObject());
        if (!JS_GetElement(cx, pair, 1, call[0]) || !JS_GetElement(cx, pair, 0, call[1])) return false;
      } else {
        call[0].set(entry);
        call[1].set(entry);
      }
      if (!JS::Call(cx, args.get(1), callback, call, &ignored)) return false;
    }
    args.rval().setUndefined();
    return true;
  }
  JS::RootedValue key(cx, args[0]);
  if (map) {
    JS::RootedString text(cx, JS::ToString(cx, key));
    if (!text) return false;
    key.setString(text);
  } else if (!RangeArgument(cx, s, where, 0, key)) {
    return false;
  }
  bool result = false;
  if (name == "has" || name == "delete") {
    if (!(name == "has" ? (map ? JS::MapHas(cx, store, key, &result) : JS::SetHas(cx, store, key, &result))
                        : (map ? JS::MapDelete(cx, store, key, &result) : JS::SetDelete(cx, store, key, &result))))
      return false;
    args.rval().setBoolean(result);
    return true;
  }
  if (name == "get") return JS::MapGet(cx, store, key, args.rval());
  if (name == "set") {
    if (!args[1].isObject()) return Fail(cx, where + ": Argument 2 is not an object.");
    if (!IsHighlight(args[1])) return Fail(cx, where + ": Argument 2 does not implement interface Highlight.");
    if (!JS::MapSet(cx, store, key, args[1])) return false;
  } else if (!JS::SetAdd(cx, store, key)) {  // add
    return false;
  }
  args.rval().setObject(*self);
  return true;
}

bool IsHighlight(JS::HandleValue value) {
  JSObject* object = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
  return object && JS::GetClass(object) == &kHighlightClass;
}

bool InstallHighlight(State& s) {
  WebKit& kit = s.kit;
  JS::RootedObject objectProto(s.cx, JS::GetRealmObjectPrototype(s.cx));
  if (!kit.Define("Highlight", &kHighlightClass, objectProto, Construct, &s.highlight)) return false;
  for (int op = Add; op <= Has; ++op)
    if (!kit.Method(s.highlight, "Highlight", kNames[op], Member, kLengths[op], op)) return false;
  JS::RootedObject proto(s.cx, s.highlight.prototype);
  JS::RootedValue values(s.cx);
  JS::RootedId iterator(s.cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(s.cx, JS::SymbolCode::iterator)));
  return kit.Accessor(s.highlight, "Highlight", "priority", Attribute, Attribute, Priority) &&
         kit.Accessor(s.highlight, "Highlight", "type", Attribute, Attribute, Type) &&
         kit.Accessor(s.highlight, "Highlight", "size", Member, nullptr, Size) &&
         JS_GetProperty(s.cx, proto, "values", &values) && JS_DefinePropertyById(s.cx, proto, iterator, values, 0);
}
}  // namespace ruyijs::web_styling
