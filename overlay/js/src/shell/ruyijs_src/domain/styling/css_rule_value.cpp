#include "domain/styling/css_rule_value.h"
#include "domain/styling/css_components.h"
#include "domain/styling/css_keyframes.h"
#include "domain/styling/css_page_value.h"
namespace ruyijs::domain::styling {
std::string CssomRuleValue::DeclarationText() const {
  return kind==6?CssPageText(PageDeclarations()):StringDeclarations();
}
std::string CssomRuleValue::Text() const {
  if(kind==0) return StringDeclarations();
  if(kind==6) {
    const auto prefix=std::string("@page")+(selector.empty()?"":" "+selector);
    const auto block=CssPageText(PageDeclarations());
    if(children.empty()) return prefix+" { "+block+(block.empty()?"":" ")+"}";
    std::string result=prefix+" {";
    if(!block.empty()) result+="\n  "+block;
    for(const auto& child:children) result+="\n  "+child->Text();
    return result+"\n}";
  }
  if(kind==7) {
    std::string result="@keyframes "+CssKeyframesNameText(selector)+" {";
    for(const auto& child:children) result+="\n"+child->Text();
    return result+"\n}";
  }
  if(kind==8) return selector+" { "+StringDeclarations()+" }";
  if(kind==1&&children.empty()) return selector+" { "+StringDeclarations()+(StringDeclarations().empty()?"":" ")+"}";
  std::string result=kind==1?selector:kind==4?"@media "+media.text():"@supports "+condition;
  result+=" {";
  if(kind==1&&!StringDeclarations().empty()) result+="\n  "+StringDeclarations();
  for(const auto& child:children) result+="\n  "+child->Text();
  return result+"\n}";
}
}
