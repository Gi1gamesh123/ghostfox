#include "domain/styling/css_condition_value.h"
#include "domain/styling/css_components.h"
namespace ruyijs::domain::styling {
namespace {
std::string FormatMedia(std::string_view value,unsigned depth=0) {
  if(depth>64) return "";
  std::string result;bool space=false;
  for(size_t p=0;p<value.size();++p) {
    char c=value[p];
    if(CssSpace(c)) {space=true;continue;}
    if(c=='(') {
      auto close=CssClosing(value,p);if(close==value.size()) return "";
      auto inner=CssTrim(value.substr(p+1,close-p-1));
      auto feature=CssMediaFeature(inner,{});
      if(feature.known) {
        auto colon=CssDelimiter(inner,0,":");
        if(colon<inner.size()) {
          auto name=CssTrim(inner.substr(0,colon)),tail=CssTrim(inner.substr(colon+1));
          if(tail=="0"&&(name.find("width")!=name.npos||name.find("height")!=name.npos)) inner=name+": 0px";
        }
        auto slash=CssDelimiter(inner,0,"/");
        if(slash<inner.size()) inner=CssTrim(inner.substr(0,slash))+" / "+CssTrim(inner.substr(slash+1));
      }
      if(space&&!result.empty()&&result.back()!=' ') result+=' ';
      result+='(';result+=FormatMedia(inner,depth+1);result+=')';p=close;space=false;continue;
    }
    if(c==':') {
      while(!result.empty()&&result.back()==' ') result.pop_back();
      result+=": ";space=false;continue;
    }
    if(space&&!result.empty()&&result.back()!=' ') result+=' ';
    result+=c;space=false;
  }
  return result;
}
}
std::string CssNormalizeMediaQuery(const std::string& query,bool* valid) {
  const auto clean=CssLower(CssClean(query));
  bool ok=CssMediaQuery(clean,{}).valid;
  if(valid) *valid=ok;
  return ok?FormatMedia(clean):"not all";
}
}
