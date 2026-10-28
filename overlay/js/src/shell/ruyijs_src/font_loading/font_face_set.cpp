#include "font_loading/internal.h"

#include "interaction/realm.h"
#include "runtime/page_helpers.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/Promise.h"
#include "js/String.h"
#include "jsapi.h"
#include "jsfriendapi.h"

namespace ruyijs::font_loading {

const JSClass kFontFaceSetClass = {"FontFaceSet", JSCLASS_HAS_RESERVED_SLOTS(kSetSlotCount)};

bool SetReceiver(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleObject self) {
  if (!args.thisv().isObject() ||
      JS::GetClass(&args.thisv().toObject()) != &kFontFaceSetClass)
    return Fail(cx, JSEXN_TYPEERR, "FontFaceSet member called on an incompatible receiver");
  self.set(&args.thisv().toObject());
  return true;
}

namespace {

bool StatusIs(JSContext* cx, JS::HandleObject object, int slot, const char* expected,
              bool* equal) {
  JS::RootedValue value(cx, JS::GetReservedSlot(object, slot));
  if (!value.isString()) { *equal = false; return true; }
  return JS_StringEqualsAscii(cx, value.toString(), expected, equal);
}

bool FaceIndex(JSContext* cx, JS::HandleObject faces, JS::HandleObject face,
               uint32_t* found) {
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, faces, &length)) return false;
  for (uint32_t index = 0; index < length; ++index) {
    JS::RootedValue value(cx);
    if (!JS_GetElement(cx, faces, index, &value)) return false;
    if (value.isObject() && &value.toObject() == face) { *found = index; return true; }
  }
  *found = length;
  return true;
}

bool AppendFace(JSContext* cx, JS::HandleObject faces, JS::HandleObject face) {
  uint32_t index = 0;
  if (!FaceIndex(cx, faces, face, &index)) return false;
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, faces, &length)) return false;
  if (index < length) return true;
  return JS_SetElement(cx, faces, length, face);
}

bool SetConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  (void)args;
  return Fail(cx, JSEXN_TYPEERR, "Illegal constructor");
}

bool Add(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!SetReceiver(cx, args, &self)) return false;
  if (args.length() == 0 || !args[0].isObject() || !IsFontFace(&args[0].toObject()))
    return Fail(cx, JSEXN_TYPEERR, "FontFaceSet.add requires a FontFace");
  JS::RootedObject face(cx, &args[0].toObject());
  JS::RootedObject faces(cx, &JS::GetReservedSlot(self, kSetFaces).toObject());
  if (!AppendFace(cx, faces, face)) return false;
  bool unloaded = false;
  if (!StatusIs(cx, face, kFontStatus, "unloaded", &unloaded)) return false;
  if (unloaded) {
    auto* events = static_cast<interaction::Realm*>(
        js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
    if (!events) return Fail(cx, JSEXN_TYPEERR, "FontFaceSet loading owner is not installed");
    JS::RootedString loading(cx, JS_NewStringCopyZ(cx, "loading"));
    JS::RootedString loaded(cx, JS_NewStringCopyZ(cx, "loaded"));
    if (!loading || !loaded) return false;
    if (!events->Fire(self, "loading", false, false)) return false;
    JS::SetReservedSlot(self, kSetStatus, JS::StringValue(loading));
    JS::RootedObject ready(cx, JS::NewPromiseObject(cx, nullptr));
    if (!ready) return false;
    JS::SetReservedSlot(self, kSetReady, JS::ObjectValue(*ready));
    JS::RootedObject promise(cx);
    if (!LoadFontFace(cx, face, &promise)) return false;
    // The set never exposes the face promise, so it must not surface as an
    // unhandled rejection: the loadingdone/loadingerror events report it.
    JS::RootedObject none(cx);
    if (!JS::AddPromiseReactionsIgnoringUnhandledRejection(cx, promise, none, none)) return false;
    JS::RootedValue result(cx, JS::ObjectValue(*self));
    if (!JS::ResolvePromise(cx, ready, result)) return false;
    JS::SetReservedSlot(self, kSetStatus, JS::StringValue(loaded));
    bool good = false;
    if (!StatusIs(cx, face, kFontStatus, "loaded", &good)) return false;
    if (!events->Fire(self, good ? "loadingdone" : "loadingerror", false, false)) return false;
  }
  args.rval().setObject(*self);
  return true;
}

bool Delete(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!SetReceiver(cx, args, &self)) return false;
  if (args.length() == 0 || !args[0].isObject() || !IsFontFace(&args[0].toObject()))
    return Fail(cx, JSEXN_TYPEERR, "FontFaceSet.delete requires a FontFace");
  JS::RootedObject face(cx, &args[0].toObject());
  JS::RootedObject faces(cx, &JS::GetReservedSlot(self, kSetFaces).toObject());
  uint32_t index = 0;
  if (!FaceIndex(cx, faces, face, &index)) return false;
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, faces, &length)) return false;
  if (index >= length) { args.rval().setBoolean(false); return true; }
  for (uint32_t at = index; at + 1 < length; ++at) {
    JS::RootedValue next(cx);
    if (!JS_GetElement(cx, faces, at + 1, &next) || !JS_SetElement(cx, faces, at, next)) return false;
  }
  if (!JS::SetArrayLength(cx, faces, length - 1)) return false;
  args.rval().setBoolean(true);
  return true;
}

bool Has(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!SetReceiver(cx, args, &self)) return false;
  if (args.length() == 0 || !args[0].isObject() || !IsFontFace(&args[0].toObject()))
    return Fail(cx, JSEXN_TYPEERR, "FontFaceSet.has requires a FontFace");
  JS::RootedObject face(cx, &args[0].toObject());
  JS::RootedObject faces(cx, &JS::GetReservedSlot(self, kSetFaces).toObject());
  uint32_t index = 0;
  if (!FaceIndex(cx, faces, face, &index)) return false;
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, faces, &length)) return false;
  args.rval().setBoolean(index < length);
  return true;
}

bool Clear(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!SetReceiver(cx, args, &self)) return false;
  JS::RootedObject faces(cx, &JS::GetReservedSlot(self, kSetFaces).toObject());
  if (!JS::SetArrayLength(cx, faces, 0)) return false;
  args.rval().setUndefined();
  return true;
}

bool SizeGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!SetReceiver(cx, args, &self)) return false;
  JS::RootedObject faces(cx, &JS::GetReservedSlot(self, kSetFaces).toObject());
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, faces, &length)) return false;
  args.rval().setNumber(length);
  return true;
}

bool StatusGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!SetReceiver(cx, args, &self)) return false;
  args.rval().set(JS::GetReservedSlot(self, kSetStatus));
  return true;
}

bool ReadyGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!SetReceiver(cx, args, &self)) return false;
  JS::Value current = JS::GetReservedSlot(self, kSetReady);
  if (current.isObject()) { args.rval().set(current); return true; }
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  JS::RootedValue result(cx, JS::ObjectValue(*self));
  if (!promise || !JS::ResolvePromise(cx, promise, result)) return false;
  args.rval().setObject(*promise);
  return true;
}

// Slot 0: the realm's one FontFaceSet; slot 1: the Document brand (Window realms).
bool FontsGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const JS::Value brand = js::GetFunctionNativeReserved(&args.callee(), 1);
  if (!brand.isUndefined() && !reinterpret_cast<ReceiverBrand>(brand.toPrivate())(args.thisv()))
    return Fail(cx, JSEXN_TYPEERR, "'get fonts' called on an object that does not implement interface Document.");
  args.rval().set(js::GetFunctionNativeReserved(&args.callee(), 0));
  return true;
}

}  // namespace

bool InstallFontFaceSet(JSContext* cx, JS::HandleObject page, JS::HandleObject document,
                        PageHelpers& helpers, interaction::Realm& events,
                        JS::HandleObject constructor, JS::HandleObject holder, ReceiverBrand isDocument) {
  JS::RootedObject event_prototype(cx, events.prototype());
  // The interface prototype object is not a FontFaceSet: its members reject it
  // as a receiver (a FontFaceSet-class prototype would pass the brand check
  // with empty slots).
  JS::RootedObject set_prototype(cx, JS_NewObjectWithGivenProto(cx, nullptr, event_prototype));
  JS::RootedObject set_constructor(cx, Function(cx, SetConstructor, "FontFaceSet", 0));
  JS::RootedObject add(cx, Function(cx, Add, "add", 1));
  JS::RootedObject remove(cx, Function(cx, Delete, "delete", 1));
  JS::RootedObject has(cx, Function(cx, Has, "has", 1));
  JS::RootedObject clear(cx, Function(cx, Clear, "clear", 0));
  JS::RootedObject load(cx, Function(cx, Load, "load", 1));
  JS::RootedObject check(cx, Function(cx, Check, "check", 1));
  JS::RootedObject size(cx, Function(cx, SizeGet, "get size", 0));
  JS::RootedObject status(cx, Function(cx, StatusGet, "get status", 0));
  JS::RootedObject ready(cx, Function(cx, ReadyGet, "get ready", 0));
  JS::RootedObject fonts(cx, Function(cx, FontsGet, "get fonts", 0));
  if (!set_prototype || !set_constructor || !add || !remove || !has || !clear || !load || !check ||
      !size || !status || !ready || !fonts ||
      !JS_DefineProperty(cx, set_constructor, "prototype", set_prototype,
                         JSPROP_READONLY | JSPROP_PERMANENT) ||
      !JS_DefineProperty(cx, set_prototype, "constructor", set_constructor, 0) ||
      !Tag(cx, set_prototype, "FontFaceSet") ||
      !JS_DefineProperty(cx, set_prototype, "add", add, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, set_prototype, "delete", remove, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, set_prototype, "has", has, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, set_prototype, "clear", clear, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, set_prototype, "load", load, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, set_prototype, "check", check, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, set_prototype, "size", size, nullptr, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, set_prototype, "status", status, nullptr, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, set_prototype, "ready", ready, nullptr, JSPROP_ENUMERATE) ||
      !InstallFontFaceSetIterators(cx, set_prototype, helpers)) return false;
  js::SetFunctionNativeReserved(add, 0, JS::PrivateValue(&events));
  JS::RootedObject faces(cx, JS::NewArrayObject(cx, 0));
  JS::RootedObject set(cx, JS_NewObjectWithGivenProto(cx, &kFontFaceSetClass, set_prototype));
  JS::RootedString loaded(cx, JS_NewStringCopyZ(cx, "loaded"));
  if (!faces || !set || !loaded ||
      !events.Attach(set)) return false;
  JS::SetReservedSlot(set, kSetFaces, JS::ObjectValue(*faces));
  JS::SetReservedSlot(set, kSetReady, JS::UndefinedValue());
  JS::SetReservedSlot(set, kSetStatus, JS::StringValue(loaded));
  js::SetFunctionNativeReserved(fonts, 0, JS::ObjectValue(*set));
  if (isDocument) js::SetFunctionNativeReserved(fonts, 1, JS::PrivateValue(reinterpret_cast<void*>(isDocument)));
  // WebIDL exposes document.fonts through the realm's Document prototype, not as
  // an own property of each document wrapper, so a clone's JSON and own keys are
  // unchanged while every document in this realm still sees the same set.
  // A Worker realm passes its WorkerGlobalScope prototype instead (holder).
  // The document's chain is HTMLDocument.prototype -> Document.prototype; the
  // member belongs to the latter (Firefox Document.webidl), one level above.
  JS::RootedObject document_prototype(cx), html_prototype(cx);
  if ((holder ? (document_prototype = holder, false)
              : !JS_GetPrototype(cx, document, &html_prototype) ||
                !JS_GetPrototype(cx, html_prototype, &document_prototype) || !document_prototype) ||
      !JS_DefineProperty(cx, document_prototype, "fonts", fonts, nullptr,
                         JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, page, "FontFaceSet", set_constructor, 0) ||
      !helpers.Member(add, "FontFaceSet", "add", "call") ||
      !helpers.Member(remove, "FontFaceSet", "delete", "call") ||
      !helpers.Member(has, "FontFaceSet", "has", "call") ||
      !helpers.Member(clear, "FontFaceSet", "clear", "call") ||
      !helpers.Member(load, "FontFaceSet", "load", "call") ||
      !helpers.Member(check, "FontFaceSet", "check", "call") ||
      !helpers.Member(size, "FontFaceSet", "get size", "get") ||
      !helpers.Member(status, "FontFaceSet", "get status", "get") ||
      !helpers.Member(ready, "FontFaceSet", "get ready", "get") ||
      !helpers.Member(fonts, holder ? "WorkerGlobalScope" : "Document", "get fonts", "get") ||
      !helpers.Interface("FontFaceSet", set_prototype) ||
      !helpers.Constructor("FontFaceSet", set_constructor)) return false;
  (void)constructor;
  return true;
}

}  // namespace ruyijs::font_loading
