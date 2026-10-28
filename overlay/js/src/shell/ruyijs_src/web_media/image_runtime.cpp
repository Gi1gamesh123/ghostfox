#include "web_media/image_decoder.h"
#include "runtime/page_reference.h"
#include "runtime/runtime.h"
#include "js/ArrayBuffer.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/GlobalObject.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/SharedArrayBuffer.h"
#include "js/Wrapper.h"
#include "js/experimental/TypedData.h"
#include <cmath>

namespace ruyijs::web_media {
bool ImageBrand(JSContext* cx, JS::HandleValue value, const JSClass* clasp,
    const char* member, JS::MutableHandleObject result) {
  result.set(value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  if (result && JS::GetClass(result) == clasp) return true;
  const std::string message = std::string("'") + member + "' called on an object that does not implement interface " + clasp->name + ".";
  return WebKit::Fail(cx, message.c_str());
}
bool ImageFactories(JSContext* cx, JS::HandleObject global, JS::MutableHandleObject out) {
  JS::RootedValue value(cx);
  if (!ReadPageReference(global, PageReference::MediaImageFactories, &value) || !value.isObject())
    return WebKit::Fail(cx, "WebMedia image factory realm is not installed");
  out.set(&value.toObject()); return JS_WrapObject(cx, out);
}
bool ImageFactoryPrototype(JSContext* cx, JS::HandleObject factories,
    unsigned index, JS::MutableHandleObject out) {
  JS::RootedValue value(cx);
  if (!JS_GetElement(cx, factories, index, &value) || !value.isObject()) return false;
  out.set(&value.toObject()); return JS_WrapObject(cx, out);
}
bool ImageDesiredPrototype(JSContext* cx, const JS::CallArgs& args,
    unsigned index, JS::MutableHandleObject out) {
  JS::RootedObject target(cx, &args.newTarget().toObject());
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, target, "prototype", &value)) return false;
  if (value.isObject()) { out.set(&value.toObject()); return true; }
  JS::Realm* realm = JS::GetFunctionRealm(cx, target);
  if (!realm) return false;
  JS::RootedObject global(cx, JS::GetRealmGlobalOrNull(realm)), factories(cx);
  return ImageFactories(cx, global, &factories) && ImageFactoryPrototype(cx, factories, index, out);
}
bool ImageException(JSContext* cx, JS::HandleObject factories,
    const char* name, const char* message) {
  JS::RootedValue constructor(cx);
  if (!JS_GetElement(cx, factories, IFException, &constructor)) return false;
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, message)), label(cx, JS_NewStringCopyZ(cx, name));
  JS::RootedObject error(cx); JS::RootedValueArray<2> values(cx);
  if (!text || !label) return false;
  values[0].setString(text); values[1].setString(label);
  if (!JS::Construct(cx, constructor, values, &error)) return false;
  JS::RootedValue reason(cx, JS::ObjectValue(*error)); JS_SetPendingException(cx, reason); return false;
}
bool ImageReject(JSContext* cx, JS::HandleObject promise) {
  JS::RootedValue reason(cx);
  if (JS_IsThrowingOutOfMemory(cx) || !JS_GetPendingException(cx, &reason)) return false;
  JS_ClearPendingException(cx); return JS::RejectPromise(cx, promise, reason);
}
bool ImageString(JSContext* cx, JS::HandleValue value, std::string* out) {
  JS::RootedString text(cx, JS::ToString(cx, value));
  return text && ToUTF8(cx, text, out);
}
bool ImageBuffer(JSContext* cx, JS::HandleValue value, std::vector<uint8_t>* bytes) {
  JS::RootedObject object(cx, value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  if (!object || (!JS_IsArrayBufferViewObject(object) && !JS::IsArrayBufferObject(object) && !JS::IsSharedArrayBufferObject(object)))
    return WebKit::Fail(cx, "Expected an ArrayBuffer or ArrayBufferView");
  bool shared = false, copied = false;
  {
    JS::AutoCheckCannotGC nogc;
    uint8_t* data = nullptr; size_t size = 0;
    if (JS_IsArrayBufferViewObject(object)) {
      data = static_cast<uint8_t*>(JS_GetArrayBufferViewData(object, &shared, nogc)); size = JS_GetArrayBufferViewByteLength(object);
    } else if (JS::IsSharedArrayBufferObject(object)) {
      data = JS::GetSharedArrayBufferData(object, &shared, nogc); size = JS::GetSharedArrayBufferByteLength(object);
    } else { data = JS::GetArrayBufferData(object, &shared, nogc); size = JS::GetArrayBufferByteLength(object); }
    copied = ImageAppendBytes(bytes, data, size);
  }
  if (!copied) { JS_ReportOutOfMemory(cx); return false; }
  return true;
}
bool ImageUnsigned(JSContext* cx, JS::HandleValue value, const char* field, uint32_t* out) {
  double number;
  if (!JS::ToNumber(cx, value, &number)) return false;
  if (!std::isfinite(number) || std::trunc(number) < 0 || std::trunc(number) > UINT32_MAX)
    return WebKit::Fail(cx, (std::string(field) + " is outside the unsigned long range").c_str());
  *out = uint32_t(number); return true;
}
}
