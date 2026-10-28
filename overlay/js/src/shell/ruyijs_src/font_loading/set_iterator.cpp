#include "font_loading/internal.h"

#include "runtime/page_helpers.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/PropertyAndElement.h"
#include "js/ValueArray.h"
#include "js/String.h"
#include "jsapi.h"
#include "jsfriendapi.h"

namespace ruyijs::font_loading {
namespace {

const JSClass kIteratorClass = {"FontFaceSet Iterator", JSCLASS_HAS_RESERVED_SLOTS(3)};

// Iteration kinds: 0 yields the face itself (keys/values), 1 yields [face, face]
// like the spec's iterable<FontFace> entries.
constexpr int32_t kIterateValues = 0;
constexpr int32_t kIterateEntries = 1;

bool IteratorReceiver(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleObject self) {
  if (!args.thisv().isObject() ||
      JS::GetClass(&args.thisv().toObject()) != &kIteratorClass)
    return Fail(cx, JSEXN_TYPEERR, "FontFaceSet iterator called on an incompatible receiver");
  self.set(&args.thisv().toObject());
  return true;
}

bool Step(JSContext* cx, JS::HandleObject iterator, JS::MutableHandleValue result) {
  JS::RootedObject faces(cx, &JS::GetReservedSlot(iterator, 0).toObject());
  const int32_t index = JS::GetReservedSlot(iterator, 1).toInt32();
  const int32_t kind = JS::GetReservedSlot(iterator, 2).toInt32();
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, faces, &length)) return false;
  JS::RootedObject entry(cx, JS_NewPlainObject(cx));
  if (!entry) return false;
  if (static_cast<uint32_t>(index) >= length) {
    JS::RootedValue done(cx, JS::BooleanValue(true));
    if (!JS_DefineProperty(cx, entry, "value", JS::UndefinedHandleValue, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, entry, "done", done, JSPROP_ENUMERATE)) return false;
  } else {
    JS::RootedValue face(cx);
    if (!JS_GetElement(cx, faces, index, &face)) return false;
    JS::RootedValue value(cx, face);
    if (kind == kIterateEntries) {
      JS::RootedObject pair(cx, JS::NewArrayObject(cx, 0));
      if (!pair || !JS_SetElement(cx, pair, 0, face) || !JS_SetElement(cx, pair, 1, face))
        return false;
      value.setObject(*pair);
    }
    JS::RootedValue done(cx, JS::BooleanValue(false));
    if (!JS_DefineProperty(cx, entry, "value", value, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, entry, "done", done, JSPROP_ENUMERATE)) return false;
    JS::SetReservedSlot(iterator, 1, JS::Int32Value(index + 1));
  }
  result.setObject(*entry);
  return true;
}

bool IteratorNext(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!IteratorReceiver(cx, args, &self)) return false;
  JS::RootedValue result(cx);
  if (!Step(cx, self, &result)) return false;
  args.rval().set(result);
  return true;
}

bool IteratorSelf(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!IteratorReceiver(cx, args, &self)) return false;
  args.rval().setObject(*self);
  return true;
}

JSObject* NewIterator(JSContext* cx, JS::HandleObject faces, JS::HandleObject prototype,
                      int32_t kind) {
  JS::RootedObject iterator(cx, JS_NewObjectWithGivenProto(cx, &kIteratorClass, prototype));
  if (!iterator) return nullptr;
  JS::SetReservedSlot(iterator, 0, JS::ObjectValue(*faces));
  JS::SetReservedSlot(iterator, 1, JS::Int32Value(0));
  JS::SetReservedSlot(iterator, 2, JS::Int32Value(kind));
  return iterator;
}

JSObject* Faces(JSContext* cx, JS::HandleObject self) {
  return &JS::GetReservedSlot(self, kSetFaces).toObject();
}

bool Iterate(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleValue result) {
  JS::RootedObject self(cx);
  if (!SetReceiver(cx, args, &self)) return false;
  JS::RootedObject prototype(cx, &js::GetFunctionNativeReserved(&args.callee(), 0).toObject());
  const int32_t kind = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  JS::RootedObject faces(cx, Faces(cx, self));
  JS::RootedObject iterator(cx, NewIterator(cx, faces, prototype, kind));
  if (!iterator) return false;
  result.setObject(*iterator);
  return true;
}

bool Values(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedValue result(cx);
  if (!Iterate(cx, args, &result)) return false;
  args.rval().set(result);
  return true;
}

bool Keys(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedValue result(cx);
  if (!Iterate(cx, args, &result)) return false;
  args.rval().set(result);
  return true;
}

bool Entries(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedValue result(cx);
  if (!Iterate(cx, args, &result)) return false;
  args.rval().set(result);
  return true;
}

bool ForEach(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!SetReceiver(cx, args, &self)) return false;
  if (args.length() == 0 || !args[0].isObject() || !JS::IsCallable(&args[0].toObject()))
    return Fail(cx, JSEXN_TYPEERR, "FontFaceSet.forEach requires a callback");
  JS::RootedObject callback(cx, &args[0].toObject());
  JS::RootedObject faces(cx, Faces(cx, self));
  JS::RootedValue this_arg(cx, JS::UndefinedValue());
  if (args.length() >= 2) this_arg.set(args[1]);
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, faces, &length)) return false;
  for (uint32_t index = 0; index < length; ++index) {
    JS::RootedValue face(cx);
    if (!JS_GetElement(cx, faces, index, &face)) return false;
    JS::RootedValue result(cx);
    // The callback receives (value, value, set), matching the IDL set callback.
    JS::RootedValueArray<3> call_args(cx);
    call_args[0].set(face);
    call_args[1].set(face);
    call_args[2].setObject(*self);
    if (!JS::Call(cx, this_arg, callback, call_args, &result)) return false;
  }
  args.rval().setUndefined();
  return true;
}

}  // namespace

bool InstallFontFaceSetIterators(JSContext* cx, JS::HandleObject setPrototype,
                                 PageHelpers& helpers) {
  JS::RootedObject prototype(cx, JS_NewPlainObject(cx));
  JS::RootedObject next(cx, Function(cx, IteratorNext, "next", 0));
  JS::RootedObject self(cx, Function(cx, IteratorSelf, "values", 0));
  if (!prototype || !next || !self ||
      !JS_DefineProperty(cx, prototype, "next", next, JSPROP_ENUMERATE) ||
      !Tag(cx, prototype, "FontFaceSet Iterator")) return false;
  JS::RootedId iterator_id(cx, JS::PropertyKey::Symbol(
      JS::GetWellKnownSymbol(cx, JS::SymbolCode::iterator)));
  if (!JS_DefinePropertyById(cx, prototype, iterator_id, self, JSPROP_ENUMERATE)) return false;
  JS::RootedObject values(cx, Function(cx, Values, "values", 0));
  JS::RootedObject keys(cx, Function(cx, Keys, "keys", 0));
  JS::RootedObject entries(cx, Function(cx, Entries, "entries", 0));
  JS::RootedObject for_each(cx, Function(cx, ForEach, "forEach", 1));
  if (!values || !keys || !entries || !for_each) return false;
  js::SetFunctionNativeReserved(values, 0, JS::ObjectValue(*prototype));
  js::SetFunctionNativeReserved(values, 1, JS::Int32Value(kIterateValues));
  js::SetFunctionNativeReserved(keys, 0, JS::ObjectValue(*prototype));
  js::SetFunctionNativeReserved(keys, 1, JS::Int32Value(kIterateValues));
  js::SetFunctionNativeReserved(entries, 0, JS::ObjectValue(*prototype));
  js::SetFunctionNativeReserved(entries, 1, JS::Int32Value(kIterateEntries));
  return JS_DefineProperty(cx, setPrototype, "values", values, JSPROP_ENUMERATE) &&
         JS_DefineProperty(cx, setPrototype, "keys", keys, JSPROP_ENUMERATE) &&
         JS_DefineProperty(cx, setPrototype, "entries", entries, JSPROP_ENUMERATE) &&
         JS_DefineProperty(cx, setPrototype, "forEach", for_each, JSPROP_ENUMERATE) &&
         JS_DefinePropertyById(cx, setPrototype, iterator_id, values, JSPROP_ENUMERATE) &&
         helpers.Member(values, "FontFaceSet", "values", "call") &&
         helpers.Member(keys, "FontFaceSet", "keys", "call") &&
         helpers.Member(entries, "FontFaceSet", "entries", "call") &&
         helpers.Member(for_each, "FontFaceSet", "forEach", "call");
}

}  // namespace ruyijs::font_loading
