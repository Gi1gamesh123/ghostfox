#include "domain/styling/css_rule_value.h"
#include "domain/styling/css_components.h"
#include "domain/styling/css_declarations.h"
#include "domain/styling/css_selector.h"
#include "domain/styling/css_keyframes.h"
#include "domain/styling/css_page_value.h"
namespace ruyijs::domain::styling {
namespace {
class RuleParser {
 public:
  bool strict=false,valid=true;
  size_t nodes=0;
  std::vector<std::shared_ptr<CssomRuleValue>> Rules(std::string_view text,unsigned depth=0,
                                                   std::string* declarations=nullptr,bool nested=false) {
    std::vector<std::shared_ptr<CssomRuleValue>> result;
    if(depth>64) { valid=false;return result; }
    std::string block;
    auto flush=[&]() {
      auto value=CssDeclarationText(block);block.clear();
      if(value.empty()) return;
      if(declarations&&result.empty()) { *declarations=value;return; }
      auto row=std::make_shared<CssomRuleValue>();row->kind=0;row->declarations=std::move(value);
      result.push_back(std::move(row));++nodes;
    };
    for(size_t p=0;(p=CssSkip(text,p))<text.size();) {
      const auto delimiter=CssDelimiter(text,p,";{");
      const auto rawPrelude=text.substr(p,delimiter-p);
      auto prelude=CssClean(rawPrelude);
      if(delimiter==text.size()||text[delimiter]==';') {
        if(nested) block+=std::string(text.substr(p,delimiter-p))+";";
        else if(!prelude.empty()&&prelude[0]!='@') valid=false;
        else if(strict&&!prelude.empty()) valid=false;
        p=delimiter==text.size()?delimiter:delimiter+1;continue;
      }
      const auto close=CssClosing(text,delimiter);
      // An EOF closes a CSS block implicitly; no repeated scans after EOF.
      auto body=text.substr(delimiter+1,close-delimiter-1);
      p=close==text.size()?close:close+1;
      if(nested&&prelude.substr(0,2)=="--"&&prelude.find(':')!=prelude.npos) {
        block+=prelude+"{"+std::string(body)+"};";continue;
      }
      flush();
      auto row=std::make_shared<CssomRuleValue>();
      if(++nodes>100000) { valid=false;break; }
      const auto lower=CssLower(prelude);
      if(lower.substr(0,6)=="@media"&&(lower.size()==6||CssSpace(lower[6])||lower[6]=='(')) {
        row->kind=4;row->media.set(CssTrim(std::string_view(prelude).substr(6)));
        row->children=Rules(body,depth+1,nullptr,nested);
      } else if(lower.substr(0,9)=="@supports"&&(lower.size()==9||CssSpace(lower[9])||lower[9]=='(')) {
        row->kind=12;row->condition=CssTrim(std::string_view(prelude).substr(9));
        row->children=Rules(body,depth+1,nullptr,nested);
      } else if((lower.starts_with("@keyframes")&&lower.size()>10&&CssSpace(lower[10]))||
                (lower.starts_with("@-webkit-keyframes")&&lower.size()>18&&CssSpace(lower[18]))) {
        size_t prefix=lower[1]=='-'?18:10;
        if(!CssKeyframesName(prelude.substr(prefix),&row->selector)) {if(strict) valid=false;continue;}
        row->kind=7;auto frames=ParseCssKeyframes(std::string(body));
        nodes+=frames.rules.size();if(nodes>100000) {valid=false;break;}
        row->children=std::move(frames.rules);
      } else if(CssPagePrelude(rawPrelude,&row->selector)) {
        row->kind=6;row->declarations=CssPageBody(body);
} else if(!prelude.empty()&&prelude[0]!='@'&&CssSelectorSyntax(prelude,nested)) {
        row->nested=nested;
        row->selector=nested?CssNestedSelectorText(prelude):CssSelectorText(prelude);
        row->children=Rules(body,depth+1,&row->StringDeclarations(),true);
      } else { if(strict) valid=false;continue; }
      result.push_back(std::move(row));
    }
    flush();
    return result;
  }
};
}
CssRuleParseResult ParseCssRuleValues(const std::string& text,bool strict) {
  if(text.size()>8*1024*1024) return {{},false};
  RuleParser parser;parser.strict=strict;
  auto rows=parser.Rules(text);
  return {std::move(rows),parser.valid};
}
}
