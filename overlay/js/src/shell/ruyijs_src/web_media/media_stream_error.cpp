// MediaStreamError, the error object Firefox 158 hands media callbacks and
// rejections (mozGetUserMedia's error callback, applyConstraints). It has no
// global interface: its prototype inherits the realm's Error.prototype and
// carries name/message/constraint accessors and the toStringTag; the instance
// has no own property. One prototype per realm, made on first use.
// Measured: artifacts/media-track/mse, artifacts/media-track/overconstrained.
#include "web_media/media_runtime.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Symbol.h"
#include "js/Wrapper.h"

namespace ruyijs::web_media {
namespace {
enum ErrorSlot { ErrorName, ErrorMessage, ErrorConstraint, ErrorSlotCount };
const JSClass kMediaStreamErrorClass = {"MediaStreamError", JSCLASS_HAS_RESERVED_SLOTS(ErrorSlotCount)};
constexpr const char* kFields[] = {"name", "message", "constraint"};

bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JSObject* self = args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr;
  if (!self || JS::GetClass(self) != &kMediaStreamErrorClass)
    return WebKit::FailBrand(cx, args, "MediaStreamError");
  args.rval().set(JS::GetReservedSlot(self, WebKit::Slot(args)));
  return JS_WrapValue(cx, args.rval());
}

bool Prototype(State& s, JS::MutableHandleObject out) {
  JSContext* cx = s.cx;
  if (s.mediaStreamErrorPrototype) { out.set(s.mediaStreamErrorPrototype); return true; }
  JS::RootedValue constructor(cx), errorProto(cx);
  if (!JS_GetProperty(cx, s.kit.page, "Error", &constructor) || !constructor.isObject()) return false;
  JS::RootedObject errorCtor(cx, &constructor.toObject());
  if (!JS_GetProperty(cx, errorCtor, "prototype", &errorProto) || !errorProto.isObject()) return false;
  JS::RootedObject parent(cx, &errorProto.toObject());
  JS::RootedObject proto(cx, JS_NewObjectWithGivenProto(cx, nullptr, parent));
  if (!proto) return false;
  WebFace face(cx);
  face.prototype = proto;
  for (int i = 0; i < ErrorSlotCount; ++i)
    if (!s.kit.Accessor(face, "MediaStreamError", kFields[i], Get, nullptr, i)) return false;
  JS::RootedString tag(cx, JS_NewStringCopyZ(cx, "MediaStreamError"));
  JS::RootedId tagId(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::toStringTag)));
  if (!tag || !JS_DefinePropertyById(cx, proto, tagId, tag, JSPROP_READONLY)) return false;
  s.mediaStreamErrorPrototype = proto;
  out.set(proto);
  return true;
}
}  // namespace

bool NewMediaStreamError(State& s, const char* name, const char* message, const char* constraint,
                         JS::MutableHandleObject out) {
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.kit.page);
  JS::RootedObject proto(cx);
  if (!Prototype(s, &proto)) return false;
  JS::RootedObject error(cx, JS_NewObjectWithGivenProto(cx, &kMediaStreamErrorClass, proto));
  const char* const values[] = {name, message, constraint};
  if (!error) return false;
  for (int i = 0; i < ErrorSlotCount; ++i) {
    JSString* text = JS_NewStringCopyZ(cx, values[i]);
    if (!text) return false;
    JS::SetReservedSlot(error, i, JS::StringValue(text));
  }
  out.set(error);
  return true;
}
}  // namespace ruyijs::web_media
