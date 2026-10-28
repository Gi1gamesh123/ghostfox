#include "domain/styling/css_page_value.h"
#include "domain/styling/css_components.h"
#include "domain/styling/css_float_text.h"
#include "domain/styling/css_numeric_value.h"
namespace ruyijs::domain::styling {
std::string CssPageLength(const std::string& value) {
  size_t p=0;double number=0;
  if(CssNumberToken(value,&p,&number)) {
    auto unit=CssLower(value.substr(p));
    return CssFloatText(float(number))+(unit.empty()?"px":unit);
  }
  const auto text=CssLower(value);
  if(!text.starts_with("calc(")) return text;
  // Resolve only absolute units. Relative/percentage expressions retain their
  // specified coordinate system; they have no layout context at this layer.
  bool absolute=true;
  for(size_t i=5;i<text.size();) {
    const auto start=i;double term;
    if(CssNumberToken(text,&i,&term)) {
      const auto units=i;
      while(i<text.size()&&((text[i]>='a'&&text[i]<='z')||text[i]=='%')) ++i;
      const auto unit=text.substr(units,i-units);
      if(!unit.empty()&&unit!="px"&&unit!="in"&&unit!="cm"&&unit!="mm"&&
          unit!="q"&&unit!="pt"&&unit!="pc") absolute=false;
    } else i=start+1;
  }
  if(absolute) {
    const auto resolved=CssNumericResolve(text,0,0,0);
    if(resolved.kind==CssLength) return "calc("+CssFloatText(float(resolved.number))+"px)";
  }
  return text;
}
}
