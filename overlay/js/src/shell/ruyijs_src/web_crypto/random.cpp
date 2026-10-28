#include "web_crypto/subtle.h"
#include "web_crypto/random_replay.h"
#include "document_model/exception.h"
#include "runtime/web_kit.h"
#include "mozilla/RandomNum.h"
#include "jsfriendapi.h"
#include "js/ArrayBuffer.h"
#include "js/Wrapper.h"
#include "js/experimental/TypedData.h"
#include <cstdio>
#include <cstring>

namespace ruyijs::web_crypto {
namespace {
bool Error(JSContext* cx, const JS::CallArgs& args, const char* name, const char* message) {
  JS::RootedObject prototype(cx, &js::GetFunctionNativeReserved(&args.callee(), 1).toObject());
  return document_model::ThrowException(cx, prototype, name, message);
}
bool IntegerView(JS::HandleObject view) {
  switch (JS_GetArrayBufferViewType(view)) {
    case js::Scalar::Int8: case js::Scalar::Uint8: case js::Scalar::Uint8Clamped:
    case js::Scalar::Int16: case js::Scalar::Uint16:
    case js::Scalar::Int32: case js::Scalar::Uint32:
    case js::Scalar::BigInt64: case js::Scalar::BigUint64: return true;
    default: return false;
  }
}
bool RandomValues(JSContext* cx, const JS::CallArgs& args, JS::HandleObject receiver) {
  JS::RootedObject view(cx, args.get(0).isObject() ? js::CheckedUnwrapStatic(&args[0].toObject()) : nullptr);
  if (!view || !JS_IsArrayBufferViewObject(view))
    return CryptoTypeError(cx, "crypto.getRandomValues requires an ArrayBuffer view");
  bool shared = false;
  JS::RootedObject buffer(cx, JS_GetArrayBufferViewBuffer(cx, view, &shared));
  if (!buffer) return false;
  if (shared) return CryptoTypeError(cx, "crypto.getRandomValues does not accept shared buffers");
  if (!IntegerView(view))
    return Error(cx, args, "TypeMismatchError", "crypto.getRandomValues requires an integer typed array");
  const size_t bytes = JS_GetTypedArrayByteLength(view);
  if (bytes > 65536)
    return Error(cx, args, "QuotaExceededError", "getRandomValues can only generate maximum 65536 bytes");
  JS::RootedObject recorded(cx);
  bool replay = false;
  if (!ReadRandomReplay(cx, receiver, bytes, false, &recorded, &replay))
    return JS_IsExceptionPending(cx) ? false : Error(cx, args, "OperationError", "Recorded Crypto input exhausted or type/byte length differs");
  // Detached integer views have length zero in Firefox and return unchanged.
  bool success = true;
  if (bytes) {
    JS::AutoCheckCannotGC nogc;
    void* data = JS_GetArrayBufferViewData(view, &shared, nogc);
    if (data && replay) {
      size_t length=0; bool sourceShared=false; uint8_t* source=nullptr;
      JS::GetArrayBufferLengthAndData(recorded, &length, &sourceShared, &source);
      std::memcpy(data, source, bytes);
    } else success = data && mozilla::GenerateRandomBytesFromOS(data, bytes);
  }
  if (!success) return Error(cx, args, "OperationError", "The operating system random generator failed");
  if (replay) CommitRandomReplay(receiver);
  args.rval().set(args[0]);
  return true;
}
bool RandomUuid(JSContext* cx, const JS::CallArgs& args, JS::HandleObject receiver) {
  uint8_t bytes[16];
  JS::RootedObject recorded(cx);
  bool replay=false;
  if(!ReadRandomReplay(cx,receiver,sizeof(bytes),true,&recorded,&replay))
    return JS_IsExceptionPending(cx)?false:Error(cx,args,"OperationError","Recorded Crypto input exhausted or method differs");
  if(replay) {
    JS::AutoCheckCannotGC nogc;
    size_t length=0;bool shared=false;uint8_t* data=nullptr;
    JS::GetArrayBufferLengthAndData(recorded,&length,&shared,&data);
    std::memcpy(bytes,data,sizeof(bytes));
  } else if (!mozilla::GenerateRandomBytesFromOS(bytes, sizeof(bytes)))
    return Error(cx, args, "OperationError", "The operating system random generator failed");
  bytes[6] = uint8_t((bytes[6] & 0x0f) | 0x40);
  bytes[8] = uint8_t((bytes[8] & 0x3f) | 0x80);
  char text[37];
  std::snprintf(text, sizeof(text),
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
      bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
  JS::RootedString value(cx, JS_NewStringCopyN(cx, text, 36));
  if (!value) return false;
  if(replay)CommitRandomReplay(receiver);
  args.rval().setString(value); return true;
}
}
bool RandomMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject receiver(cx, args.thisv().isObject() ?
      js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!receiver || JS::GetClass(receiver) != &kCryptoClassDefinition)
    return WebKit::FailBrand(cx, args, "Crypto");
  return RandomOperation(js::GetFunctionNativeReserved(&args.callee(), 0).toInt32()) == RandomOperation::Values ?
      RandomValues(cx, args, receiver) : RandomUuid(cx, args, receiver);
}
}
