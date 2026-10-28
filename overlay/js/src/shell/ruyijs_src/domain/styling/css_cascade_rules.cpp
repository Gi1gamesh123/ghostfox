// Project the single live rule tree into the existing per-compute cascade input.
#include "domain/styling/stylesheet.h"
namespace ruyijs::domain::styling {
namespace {
std::string NestedSelector(const std::string& input,const std::string& parent) {
  if(parent.empty()) return input;
  const std::string ancestor=":is("+parent+")";std::string result;
  for(const auto& group:CssSplit(input,',')) {
    std::string current;bool ampersand=false;char quote=0;
    for(size_t i=0;i<group.size();++i) {
      char c=group[i];
      if(c=='\\') {current+=c;if(i+1<group.size()) current+=group[++i];continue;}
      if(quote) {current+=c;if(c==quote) quote=0;continue;}
      if(c=='\''||c=='"') {quote=c;current+=c;continue;}
      if(c=='&') {current+=ancestor;ampersand=true;}else current+=c;
    }
    if(!result.empty()) result+=", ";
    result+=(ampersand?"":ancestor+" ")+current;
  }
  return result;
}
void Project(StyleSheetAggregate& sheet,const std::vector<std::shared_ptr<CssomRuleValue>>& rows,
              const CssMediaEnvironment& environment,const std::string& parent,unsigned depth) {
  if(depth>64||parent.size()>8*1024*1024) return;
  for(const auto& row:rows) {
    if(row->kind==4||row->kind==12) {
      bool applies=row->kind==4?CssMediaCondition(row->media.text(),environment):CssSupportsCondition(row->condition);
      if(applies) Project(sheet,row->children,environment,parent,depth+1);
      continue;
    }
    if(row->kind!=1&&row->kind!=0) continue;
    std::string selector=row->kind==0?parent:NestedSelector(row->selector,parent);
    if(!selector.empty()&&!row->StringDeclarations().empty()) {
      CssRule rule(CssRule::Type::Style,CssSelector{selector});
      for(const auto& property:CssDeclarations(row->StringDeclarations()))
        rule.set_property(property.name,property.value,property.important);
      sheet.add_rule(std::move(rule));
    }
    Project(sheet,row->children,environment,selector,depth+1);
  }
}
}
void StyleSheetAggregate::append_rule_values(const std::vector<std::shared_ptr<CssomRuleValue>>& rows,
                                            const CssMediaEnvironment& environment) {
  Project(*this,rows,environment,"",0);
}
}
