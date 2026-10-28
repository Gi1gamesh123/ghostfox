// WebMedia's standalone WebCodecs color value. Values live only in object slots.
#include "web_media/video_color_space.h"
#include "web_media/media_runtime.h"
#include "domain/web_media/video_color_space.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/runtime.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_media {
namespace {
const JSClass kColorClass = {"VideoColorSpace", JSCLASS_HAS_RESERVED_SLOTS(4)};
constexpr const char* kFields[] = {"primaries", "transfer", "matrix", "fullRange"};
constexpr const char* kEnums[] = {
    "VideoColorPrimaries", "VideoTransferCharacteristics", "VideoMatrixCoefficients"};
bool Receiver(JSContext* cx, const JS::CallArgs& args, unsigned field,
              JS::MutableHandleObject object) {
  object.set(args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (object && JS::GetClass(object) == &kColorClass) return true;
  std::string message = field < 4 ? std::string("'get ") + kFields[field] + "'" : "'toJSON'";
  message += " called on an object that does not implement interface VideoColorSpace.";
  return WebKit::Fail(cx, message.c_str());
}
bool Prototype(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleObject prototype) {
  JS::RootedObject target(cx, &args.newTarget().toObject());
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, target, "prototype", &value)) return false;
  if (value.isObject()) { prototype.set(&value.toObject()); return true; }
  JS::Realm* realm = JS::GetFunctionRealm(cx, target);
  if (!realm) return false;
  State* owner = StateForGlobal(JS::GetRealmGlobalOrNull(realm));
  if (!owner) return WebKit::Fail(cx, "VideoColorSpace target realm is not installed");
  prototype.set(owner->videoColorSpace.prototype);
  return JS_WrapObject(cx, prototype);
}
bool ConvertField(JSContext* cx, unsigned field, JS::MutableHandleValue value) {
  if (value.isNullOrUndefined()) { value.setNull(); return true; }
  if (field == 3) { value.setBoolean(JS::ToBoolean(value)); return true; }
  JS::RootedString text(cx, JS::ToString(cx, value));
  std::string encoded;
  if (!text || !ToUTF8(cx, text, &encoded)) return false;
  if (!domain::web_media::ValidColorSpaceValue(field, encoded)) {
    std::string message = "VideoColorSpace constructor: '" + encoded + "' (value of '" +
        kFields[field] + "' member of VideoColorSpaceInit) is not a valid value for enumeration " +
        kEnums[field] + ".";
    return WebKit::Fail(cx, message.c_str());
  }
  value.setString(text);
  return true;
}
bool Constructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing())
    return trace.Finish(WebKit::Fail(cx, "VideoColorSpace constructor: 'new' is required"));
  JS::RootedObject prototype(cx), dictionary(cx);
  if (!Prototype(cx, args, &prototype)) return trace.Finish(false);
  if (args.get(0).isObject()) dictionary = &args[0].toObject();
  else if (!args.get(0).isNullOrUndefined())
    return trace.Finish(WebKit::Fail(cx,
        "VideoColorSpace constructor: Argument 1 can't be converted to a dictionary."));
  JS::RootedValueArray<4> fields(cx);
  for (unsigned field : {3u, 2u, 0u, 1u}) {
    fields[field].setUndefined();
    if ((dictionary && !JS_GetProperty(cx, dictionary, kFields[field], fields[field])) ||
        !ConvertField(cx, field, fields[field])) return trace.Finish(false);
  }
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kColorClass, prototype));
  if (!object) return trace.Finish(false);
  for (unsigned field = 0; field < 4; ++field)
    JS::SetReservedSlot(object, field, fields[field]);
  args.rval().setObject(*object);
  return trace.Finish(true);
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const unsigned field = unsigned(WebKit::Slot(args));
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, field, &self)) return false;
  args.rval().set(JS::GetReservedSlot(self, field));
  return JS_WrapValue(cx, args.rval());
}
bool ToJson(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, 4, &self)) return false;
  JS::RootedObject result(cx, JS_NewPlainObject(cx));
  JS::RootedValue value(cx);
  if (!result) return false;
  for (unsigned field = 0; field < 4; ++field) {
    value.set(JS::GetReservedSlot(self, field));
    if (!JS_WrapValue(cx, &value) ||
        !JS_DefineProperty(cx, result, kFields[field], value, JSPROP_ENUMERATE)) return false;
  }
  args.rval().setObject(*result);
  return true;
}
}
bool NewVideoColorSpace(JSContext* cx, JS::HandleObject prototype, bool rgb,
                        JS::MutableHandleObject out) {
  out.set(JS_NewObjectWithGivenProto(cx, &kColorClass, prototype));
  if (!out) return false;
  for (unsigned i = 0; i < 4; ++i) JS::SetReservedSlot(out, i, JS::NullValue());
  if (rgb) {
    const char* names[] = {"bt709", "iec61966-2-1", "rgb"};
    for (unsigned i = 0; i < 3; ++i) {
      JS::RootedString text(cx, JS_NewStringCopyZ(cx, names[i]));
      if (!text) return false;
      JS::SetReservedSlot(out, i, JS::StringValue(text));
    }
    JS::SetReservedSlot(out, 3, JS::BooleanValue(true));
  }
  return true;
}
bool NewVideoColorSpaceFromInit(JSContext* cx, JS::HandleObject prototype,
    JS::HandleValue input, bool rgb, JS::MutableHandleObject out) {
  if (!NewVideoColorSpace(cx, prototype, rgb, out)) return false;
  if (input.isNullOrUndefined()) return true;
  if (!input.isObject()) return WebKit::Fail(cx,"VideoColorSpaceInit must be a dictionary");
  JS::RootedObject dictionary(cx,&input.toObject());
  for(unsigned field:{3u,2u,0u,1u}) {
    JS::RootedValue value(cx);
    if(!JS_GetProperty(cx,dictionary,kFields[field],&value))return false;
    if(value.isUndefined())continue;
    if(!ConvertField(cx,field,&value))return false;
    JS::SetReservedSlot(out,field,value);
  }
  return true;
}
bool InstallVideoColorSpace(State& state) {
  JS::RootedObject parent(state.cx, JS::GetRealmObjectPrototype(state.cx));
  WebFace& face = state.videoColorSpace;
  if (!parent || !state.kit.Define("VideoColorSpace", &kColorClass, parent, Constructor, &face) ||
      !state.kit.Method(face, "VideoColorSpace", "toJSON", ToJson, 0, 0)) return false;
  for (unsigned field = 0; field < 4; ++field)
    if (!state.kit.Accessor(face, "VideoColorSpace", kFields[field], Get, nullptr, field)) return false;
  return true;
}
}
