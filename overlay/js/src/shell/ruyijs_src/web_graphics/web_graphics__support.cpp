#include "web_graphics/internal.h"

namespace ruyijs::web_graphics::detail {
bool AsciiEquals(JSContext* cx, JS::HandleString text, const char* ascii, bool* match) {
  return JS_StringEqualsAscii(cx, text, ascii, match);
}
}  // namespace ruyijs::web_graphics::detail