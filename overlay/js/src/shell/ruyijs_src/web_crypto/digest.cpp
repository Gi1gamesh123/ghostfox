#include "web_crypto/subtle.h"
#include "web_crypto/digest_backend.h"
#include "runtime/runtime.h"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include <cstring>
namespace ruyijs::web_crypto {
namespace {
bool Name(JSContext* cx, JS::HandleValue algorithm, std::string* name) {
  JS::RootedValue value(cx, algorithm);
  if (value.isObject()) {
    JS::RootedObject object(cx, &value.toObject());
    if (!JS_GetProperty(cx, object, "name", &value)) return false;
    if (value.isUndefined()) return CryptoTypeError(cx, "WebCrypto algorithm requires a name");
  }
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text || !ToUTF8(cx, text, name)) return false;
  for (char& c : *name) if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
  return true;
}
bool Algorithm(const std::string& name, HashAlgorithm* result) {
  static const char* const names[] = {"SHA-1", "SHA-256", "SHA-384", "SHA-512"};
  for (unsigned i = 0; i < 4; ++i) if (name == names[i]) {
    *result = HashAlgorithm(i); return true;
  }
  return false;
}
}
bool Digest(JSContext* cx, JS::HandleObject self, JS::HandleValue inputAlgorithm,
            JS::HandleValue data, JS::MutableHandleValue result) {
  JS::RootedValue algorithm(cx, inputAlgorithm);
  // WebIDL converts the object-or-string union before validating BufferSource.
  if (!algorithm.isObject()) {
    JS::RootedString text(cx, JS::ToString(cx, algorithm));
    if (!text) return false;
    algorithm.setString(text);
  }
  JS::RootedObject buffer(cx);
  if (!CryptoBuffer(cx, data, &buffer)) return false;
  std::string name;
  HashAlgorithm selected;
  if (!Name(cx, algorithm, &name)) {
    if (JS_IsThrowingOutOfMemory(cx)) return false;
    JS_ClearPendingException(cx);
    return CryptoTypeError(cx, "Invalid WebCrypto algorithm dictionary");
  }
  if (!Algorithm(name, &selected)) return SubtleError(cx, self, "NotSupportedError");
  CryptoBytes bytes;
  if (!CryptoSnapshot(cx, buffer, &bytes)) return false;
  // Firefox task selection and the digest task each normalize the dictionary.
  if (algorithm.isObject() && (!Name(cx, algorithm, &name) || !Algorithm(name, &selected))) {
    if (JS_IsThrowingOutOfMemory(cx)) return false;
    JS_ClearPendingException(cx);
    return SubtleError(cx, self, "SyntaxError");
  }
  DigestBytes digest;
  if (!ComputeDigest(selected, bytes.data.get(), bytes.size, &digest))
    return SubtleError(cx, self, "OperationError");
  JS::RootedObject output(cx, JS::NewArrayBuffer(cx, digest.size));
  if (!output) return false;
  size_t length = 0; bool shared = false; uint8_t* target = nullptr;
  JS::GetArrayBufferLengthAndData(output, &length, &shared, &target);
  std::memcpy(target, digest.data, length);
  result.setObject(*output);
  return true;
}
}
