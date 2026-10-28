// Share the original generated CSS property names; never inspect public JS.
#include "web_styling/page_descriptors.h"
namespace ruyijs::web_styling {
namespace {
struct DeclarationName {const char* idl;const char* css;const char* attribute;};
constexpr DeclarationName declarationNames[]={
#include "document_model/css_properties.inc"
};
}
bool KnownDeclarationName(std::string_view name) {
  if((name.size()>2&&name.starts_with("--"))||name=="size"||name=="page-orientation") return true;
  for(const auto& property:declarationNames) if(name==property.css) return true;
  return false;
}
}
