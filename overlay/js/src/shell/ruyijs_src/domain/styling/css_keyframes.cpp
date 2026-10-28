#include "domain/styling/css_keyframes.h"
#include "domain/styling/css_float_text.h"
#include "domain/styling/css_components.h"
#include "domain/styling/css_declarations.h"
#include "domain/styling/css_identifier.h"
#include "domain/styling/css_numeric_value.h"
#include "domain/styling/css_selector_tokens.h"
#include <charconv>
#include <cmath>
namespace ruyijs::domain::styling {
bool CssKeyPositions(const std::string& input,std::vector<float>* out) {
  std::vector<float> result;
  for(const auto& part:CssSplit(input,',')) {
    const auto& token=part;double number=0;size_t p=CssSkip(token,0),end=p;std::string ident;
    if(SelectorIdent(token,&end,&ident)) {
      ident=CssLower(ident);if(ident!="from"&&ident!="to") return false;
      number=ident=="to"?100:0;p=end;
    } else {
      if(!CssNumberToken(token,&p,&number)||p==token.size()||token[p++]!='%') return false;
      // cssparser tokenizes in f64, divides by 100, then stores the f32
      // fraction. Rounding before division changes findRule identity.
      const float fraction=static_cast<float>(number/100.0);
      if(!std::isfinite(fraction)||fraction<0||fraction>1) return false;
      number=fraction;
    }
    if(CssSkip(token,p)!=token.size()) return false;
    result.push_back(static_cast<float>(ident.empty()?number:number/100));
  }
  if(result.empty()) return false;
  *out=std::move(result);return true;
}
std::string CssKeyText(const std::vector<float>& positions) {
  std::string result;
  for(float position:positions) {
    if(!result.empty()) result+=", ";
    result+=CssFloatText(position*100.0f)+"%";
  }
  return result;
}
bool CssKeyText(const std::string& input,std::string* out) {
  std::vector<float> positions;
  if(!CssKeyPositions(input,&positions)) return false;
  *out=CssKeyText(positions);return true;
}
namespace {
bool ReservedName(const std::string& name) {
  auto lower=CssLower(name);
  return lower=="none"||lower=="initial"||lower=="inherit"||lower=="unset"||
    lower=="revert"||lower=="revert-layer"||lower=="default";
}
}
bool CssKeyframesName(const std::string& text,std::string* out) {
  const auto clean=CssClean(text);size_t p=0;std::string name;
  const bool quoted=!clean.empty()&&(clean[0]=='"'||clean[0]=='\'');
  if(!(quoted?SelectorString(clean,&p,&name):SelectorIdent(clean,&p,&name))||
      p!=clean.size()||name.empty()||(!quoted&&ReservedName(name))) return false;
  *out=std::move(name);return true;
}
std::string CssKeyframesNameText(const std::string& name) {
  if(name.empty()) return "none";
  if(ReservedName(name)) return "\""+name+"\"";
  if(name=="-") return "\\-";
  return CssEscapeIdentifier(name);
}
std::string CssKeyframeDeclarations(const std::string& text) {
  std::string clean;
  // Filter before duplicate resolution: a rejected important declaration must
  // not remove an earlier valid normal declaration of the same property.
  for(const auto& part:CssSplit(text,';'))
    for(const auto& row:CssDeclarations(part))
      if(!row.important) clean+=row.name+": "+row.value+";";
  return CssDeclarationText(clean);
}
CssRuleParseResult ParseCssKeyframes(const std::string& text,bool strict) {
  CssRuleParseResult result;
  if(text.size()>8*1024*1024) return {{},false};
  for(size_t p=0;(p=CssSkip(text,p))<text.size();) {
    const auto open=CssDelimiter(text,p,";{");
    if(open==text.size()||text[open]!='{') {
      result.valid=false;p=open==text.size()?open:open+1;continue;
    }
    const auto close=CssClosing(text,open);
    std::vector<float> positions;bool valid=CssKeyPositions(text.substr(p,open-p),&positions);
    if(valid) {
      auto row=std::make_shared<CssomRuleValue>();row->kind=8;row->key_positions=std::move(positions);
      row->selector=CssKeyText(row->key_positions);
      row->declarations=CssKeyframeDeclarations(text.substr(open+1,close-open-1));
      result.rules.push_back(std::move(row));
      if(result.rules.size()>100000) return {{},false};
    } else if(strict) result.valid=false;
    p=close==text.size()?close:close+1;
  }
  return result;
}
}
