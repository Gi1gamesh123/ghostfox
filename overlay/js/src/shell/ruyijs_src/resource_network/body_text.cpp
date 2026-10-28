// Network body bytes are UTF-8, not the Latin-1 input accepted by CopyN.
#include "resource_network/state.h"
#include "js/CharacterEncoding.h"
#include "js/Conversions.h"
#include "js/String.h"
#include "js/Utility.h"

namespace ruyijs::resource_network {
bool EncodeBodyText(JSContext* cx, JS::HandleValue value, std::string* bytes) {
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text) return false;
  JSLinearString* linear = JS_EnsureLinearString(cx, text);
  if (!linear) return false;
  const size_t length = JS::GetDeflatedUTF8StringLength(linear);
  JS::UniqueChars encoded(JS_EncodeStringToUTF8(cx, text));
  if (!encoded) return false;
  bytes->assign(encoded.get(), length);
  return true;
}
JSString* DecodeBodyText(JSContext* cx, std::string_view bytes) {
  if (bytes.size() >= 3 && bytes.substr(0, 3) == "\xEF\xBB\xBF")
    bytes.remove_prefix(3);
  size_t length = 0;
  JS::UniqueTwoByteChars decoded(JS::LossyUTF8CharsToNewTwoByteCharsZ(
      cx, JS::UTF8Chars(bytes.data(), bytes.size()), &length, js::MallocArena).get());
  if (!decoded) return nullptr;
  return JS_NewUCStringCopyN(cx, decoded.get(), length);
}
}
