// Inline CSSOM input validation; the original attribute remains the only store.
#include "document_model/style_internal.h"
#include "document_model/style_source_values.h"
#include "domain/styling/css_components.h"
#include "domain/styling/css_condition_value.h"

namespace ruyijs::document_model {
bool PrepareInlineValue(DomStringView name, DomString* value) {
  if (value->empty()) return true;
  namespace css = ruyijs::domain::styling;
  const auto text = StyleSourceText(*value);
  // A value argument never carries a declaration terminator or priority.
  // Quotes, escapes, comments and nested blocks use the existing CSS scanner.
  if (css::CssDelimiter(text, 0, "!;") != text.size()) return false;
  if (name == u"font-size" || name == u"position") {
    const auto complete = css::CssValueAtEof(text);
    if (!css::CssSupportsProperty(StyleSourceText(name), complete)) return false;
    for (size_t i = text.size(); i < complete.size(); ++i)
      value->push_back(static_cast<char16_t>(complete[i]));
  }
  // The font shorthand sets every font longhand at once, so the whole value has
  // to parse: Firefox keeps the old value for anything that is neither the
  // size/family form nor a system-font keyword, so `style.font` stays empty.
  if (name == u"font" && !css::CssSupportsProperty("font", text)) return false;
  return true;
}
}  // namespace ruyijs::document_model
