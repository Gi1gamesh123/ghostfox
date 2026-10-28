#include "domain/styling/css_box_value.h"
#include "domain/styling/css_numeric_value.h"
#include "domain/styling/css_font_value.h"
#include "domain/styling/stylesheet.h"
#include <cmath>
namespace ruyijs::domain::styling {
namespace {
std::string BoxComputedValue(const StyleDeclaration& style, const std::string& key) {
  auto value = style.get_property_value(key);
  return value.empty() ? StyleResolutionService::initial_value(key) : value;
}
}
void CssComputeBox(StyleDeclaration& style, double root_size) {
  const double font_size = style.computed_font_size;
  for (const auto* side : {"top", "right", "bottom", "left"}) {
    for (const auto* group : {"margin-", "padding-"}) {
      const auto key = std::string(group)+side, text = BoxComputedValue(style,key);
      if (text == "0px") { style.set_property(key,text); continue; }
      const auto parsed = CssNumericParse(text);
      if (parsed.kind != CssLength && !(parsed.kind == CssNumber && parsed.number == 0)) continue;
      auto resolved = CssNumericResolve(text,font_size,root_size,0);
      if (!resolved.kind) continue;
      if (group[0] == 'p') resolved.number = std::max(0.0,resolved.number);
      const double rounded = std::floor(resolved.number*60+.5)/60;
      style.set_property(key,CssFontNumber(rounded)+"px");
    }
    const auto prefix = std::string("border-")+side;
    const auto border_style = BoxComputedValue(style,prefix+"-style");
    if (border_style == "none" || border_style == "hidden") {
      style.set_property(prefix+"-width","0px"); continue;
    }
    auto width = BoxComputedValue(style,prefix+"-width");
    if (width == "thin") width = "1px";
    if (width == "medium") width = "3px";
    if (width == "thick") width = "5px";
    double used = 0;
    const auto value = CssNumericResolve(width,font_size,root_size,0);
    if (value.kind && value.number > 0)
      used = std::max(1.0,std::floor(value.number));
    style.set_property(prefix+"-width",CssFontNumber(used)+"px");
  }
}
}
