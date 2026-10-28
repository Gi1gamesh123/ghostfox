// Inline style declaration-block operations (style_internal.h). The element's
// style attribute stays the single store; parsing, shorthand expansion and
// block serialization belong to the Styling owner (css_declarations.h), so an
// inline block and a rule block answer the same.
#include "document_model/style_internal.h"
#include "document_model/style_source_values.h"
#include "domain/styling/css_declarations.h"

namespace ruyijs::document_model {
namespace css = ruyijs::domain::styling;
namespace {
std::vector<css::CssDeclarationValue> Rows(DomStringView block) {
  return css::CssDeclarations(StyleSourceText(block));
}
}  // namespace
size_t InlineCount(DomStringView block) { return Rows(block).size(); }
DomString InlineItem(DomStringView block, size_t index) {
  const auto rows = Rows(block);
  return index < rows.size() ? StyleSourceWide(rows[index].name) : DomString();
}
DomString InlineValue(DomStringView block, DomStringView name, bool* important) {
  return StyleSourceWide(css::CssLookupDeclaration(Rows(block), StyleSourceText(name), important));
}
DomString InlineText(DomStringView block) { return StyleSourceWide(css::CssSerializeBlock(Rows(block))); }
DomString InlineSet(DomStringView block, DomStringView name, DomStringView value, bool important,
                    bool remove, DomString* old) {
  auto text = css::CssSerializeBlock(Rows(block));
  const auto previous = css::CssSetDeclaration(text, StyleSourceText(name), StyleSourceText(value), important, remove);
  if (old) *old = StyleSourceWide(previous);
  return StyleSourceWide(text);
}
}  // namespace ruyijs::document_model
