// Original buffered Body result conversions, shared with native stream consumers.
#include "resource_network/body_result.h"
#include "resource_network/state.h"
#include "js/ArrayBuffer.h"
#include "js/JSON.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/experimental/TypedData.h"
#include <cstring>

namespace ruyijs::resource_network {
bool BodyTextValue(JSContext* cx,const BodyRecord* body,JS::MutableHandleValue value) {
  JS::RootedString text(cx,DecodeBodyText(cx,body->bytes));
  if (!text) return false;
  value.setString(text);
  return true;
}
bool BodyJsonValue(JSContext* cx,const BodyRecord* body,JS::MutableHandleValue value,
                   bool* rejected) {
  *rejected=false;
  JS::RootedString source(cx,DecodeBodyText(cx,body->bytes));
  if (!source) return false;
  if (!JS_ParseJSON(cx,source,value)) {
    JS::RootedValue error(cx);
    if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,&error)) return false;
    JS_ClearPendingException(cx);
    value.set(error);
    *rejected=true;
  }
  return true;
}
bool BodyArrayBufferValue(JSContext* cx,const BodyRecord* body,JS::MutableHandleValue value) {
  const size_t size=body->bytes.size();
  JS::RootedObject buffer(cx,JS::NewArrayBuffer(cx,size));
  if (!buffer) return false;
  size_t length=0;
  bool shared=false;
  uint8_t* data=nullptr;
  JS::GetArrayBufferLengthAndData(buffer,&length,&shared,&data);
  if (size) std::memcpy(data,body->bytes.data(),size);
  value.setObject(*buffer);
  return true;
}
bool BodyBytesValue(JSContext* cx,const BodyRecord* body,JS::MutableHandleValue value) {
  JS::RootedValue buffer(cx);
  if (!BodyArrayBufferValue(cx,body,&buffer)) return false;
  JS::RootedObject bytes(cx,&buffer.toObject());
  JS::RootedObject array(cx,JS_NewUint8ArrayWithBuffer(cx,bytes,0,-1));
  if (!array) return false;
  value.setObject(*array);
  return true;
}
}
