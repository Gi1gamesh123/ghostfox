#include "domain/styling/css_page_value.h"
#include "domain/styling/css_box_value.h"
#include "domain/styling/css_color_value.h"
#include "domain/styling/css_components.h"
#include "domain/styling/css_numeric_value.h"
#include <algorithm>
namespace ruyijs::domain::styling {
bool CssPageWide(std::string_view value) {
  return value=="initial"||value=="inherit"||value=="unset"||value=="revert"||value=="revert-layer";
}
std::vector<std::string> CssPageLonghands(std::string_view name) {
  if(name=="margin") return CssBoxLonghands(name);
  if(name=="margin-block"||name=="margin-inline"||name=="padding-block"||name=="padding-inline")
    return {std::string(name)+"-start",std::string(name)+"-end"};
  return {};
}
bool CssPageProperty(std::string_view name) {
  return (name.size()>2&&name.substr(0,2)=="--")||name=="size"||name=="page-orientation"||
    name=="margin-top"||name=="margin-right"||name=="margin-bottom"||name=="margin-left"||
    !CssPageLonghands(name).empty();
}
namespace {
std::vector<std::string> PageTokens(const std::string& value) {
  std::vector<std::string> result;
  for(size_t p=0;(p=CssSkip(value,p))<value.size();) {
    const auto end=CssDelimiter(value,p," \t\r\n\f");
    result.push_back(value.substr(p,end-p));p=end;
    if(result.size()>4) break;
  }
  return result;
}
bool PagePaper(const std::string& value) {
  for(auto word:{"a3","a4","a5","b4","b5","jis-b4","jis-b5","letter","legal","ledger"})
    if(value==word) return true;
  return false;
}
bool PageOrientation(const std::string& value) {return value=="portrait"||value=="landscape";}
bool PageSize(std::vector<std::string> tokens,std::string* out) {
  if(tokens.empty()||tokens.size()>2) return false;
  if(tokens.size()==2&&PageOrientation(tokens[0])&&PagePaper(tokens[1])) std::swap(tokens[0],tokens[1]);
  if(PagePaper(tokens[0])) {
    if(tokens.size()==2&&!PageOrientation(tokens[1])) return false;
    *out=tokens[0]+(tokens.size()==2&&tokens[1]=="landscape"?" landscape":"");return true;
  }
  if(tokens.size()==1&&(PageOrientation(tokens[0])||tokens[0]=="auto")) {*out=tokens[0];return true;}
  for(auto& token:tokens) {
    if(!CssNumericAllowed(token,CssLength,false)) return false;
    token=CssPageLength(token);
  }
  *out=tokens[0];if(tokens.size()==2&&tokens[1]!=tokens[0]) *out+=" "+tokens[1];
  return true;
}
}
bool CssPageExpand(const std::string& name,const std::string& input,bool important,
                    std::vector<CssDeclarationValue>* out) {
  if(!CssPageProperty(name)) return false;
  const bool custom=name.starts_with("--");
  if(!CssValueComponentsValid(input,custom)) return false;
  if(custom) {out->push_back({name,CssTrim(input),important});return true;}
  std::string clean;
  for(size_t p=0;p<input.size();) {
    if(input.compare(p,2,"/*")==0) {
      const auto end=input.find("*/",p+2);p=end==input.npos?input.size():end+2;clean+=' ';
    } else clean+=input[p++];
  }
  auto value=CssLower(CssTrim(clean));if(value.empty()) return false;
  const auto names=CssPageLonghands(name);
  const bool variable=CssVariableReferenceValid(input);
  if(CssPageWide(value)||variable) {
    if(variable) value=CssTrim(input);
    if(names.empty()) out->push_back({name,value,important});
    else for(const auto& key:names) out->push_back({key,value,important,variable?name:""});
    return true;
  }
  const auto tokens=PageTokens(value);
  if(name=="size") {
    if(!PageSize(tokens,&value)) return false;
  } else if(name=="page-orientation") {
    if(value!="upright"&&value!="rotate-left"&&value!="rotate-right") return false;
  } else if(!names.empty()) {
    if(tokens.empty()||tokens.size()>names.size()) return false;
    std::vector<std::string> converted;
    const bool padding=name.starts_with("padding-");
    for(const auto& token:tokens) {
      if(token=="auto"&&!padding) converted.push_back(token);
      else {
        if(!CssNumericAllowed(token,CssLength|CssPercent,!padding)) return false;
        converted.push_back(CssPageLength(token));
      }
    }
    const size_t sides[]={0,converted.size()>1?1u:0u,converted.size()>2?2u:0u,
                          converted.size()>3?3u:converted.size()>1?1u:0u};
    for(size_t i=0;i<names.size();++i) out->push_back({names[i],converted[sides[i]],important});
    return true;
  } else if(value!="auto") {
    if(!CssNumericAllowed(value,CssLength|CssPercent)) return false;
    value=CssPageLength(value);
  }
  out->push_back({name,value,important});return true;
}
}
