#include "domain/styling/css_color_value.h"
#include "domain/styling/css_selector.h"
#include "domain/styling/css_components.h"
namespace ruyijs::domain::styling {
bool CssSelectorSupported(std::string_view input,unsigned depth) {
  auto text=CssClean(input);
  return CssSplit(text,',').size()==1&&ParsedSelector::Parse(text,depth,true).valid();
}
}
