#include "domain/styling/css_condition_value.h"
#include "domain/styling/css_components.h"
namespace ruyijs::domain::styling {
namespace {
bool MediaIdentifier(const std::string& name) {
  if(name.empty()||name=="not"||name=="only"||(name[0]>='0'&&name[0]<='9')) return false;
  for(char c:name) if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_')) return false;
  return true;
}
bool TopLevelOr(const std::string& value) {
  for(size_t p=0;p<value.size();++p) {
    if(value[p]=='(') {auto close=CssClosing(value,p);if(close==value.size()) return true;p=close;continue;}
    if(value.substr(p,4)==" or ") return true;
  }
  return false;
}
}
CssConditionResult CssMediaQuery(const std::string& query,const CssMediaEnvironment& env) {
  auto group=CssLower(CssClean(query));
  if(group.empty()) return {};
  bool negate=false,only=false;
  if(group.substr(0,4)=="not ") {negate=true;group=CssTrim(std::string_view(group).substr(4));}
  else if(group.substr(0,5)=="only ") {only=true;group=CssTrim(std::string_view(group).substr(5));}
  auto atom=[&](std::string_view value){return CssMediaFeature(value,env);};
  if(group.empty()) return {};
  if(group.front()=='('||group.find('(')<group.find(' ')) {
    if(only) return {};
    return CssBooleanCondition(negate?"not "+group:group,atom);
  }
  auto split=group.find(' ');auto name=group.substr(0,split);
  if(!MediaIdentifier(name)) return {};
  bool type=name=="all"||name=="screen";
  if(split==group.npos) return {true,negate?!type:type};
  auto rest=CssTrim(std::string_view(group).substr(split));
  if(rest.substr(0,4)!="and ") return {};
  rest=CssTrim(std::string_view(rest).substr(4));
  if(rest.empty()||rest.front()!='('||TopLevelOr(rest)) return {};
  auto condition=CssBooleanCondition(rest,atom);if(!condition.valid) return {};
  if(!type) condition={true,false};
  if(negate) condition.value=!condition.value;
  return condition;
}
bool CssMediaCondition(const std::string& query,const CssMediaEnvironment& env) {
  if(CssTrim(query).empty()) return true;
  for(const auto& group:CssSplit(query,',')) {
    auto answer=CssMediaQuery(group,env);
    if(answer.valid&&answer.known&&answer.value) return true;
  }
  return false;
}
}
