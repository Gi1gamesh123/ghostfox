#include "domain/styling/css_selector.h"
#include "domain/styling/css_components.h"
namespace ruyijs::domain::styling {
bool ParsedSelector::MatchFunctions(const ElementNode& node,const SelectorCompound& part,
                                   const ElementNode* scope,size_t& budget,unsigned depth) {
  for(const auto& fn:part.functions) {
    // The DOM scope adapter currently supplies no host marker for a light-tree
    // element.  Keep :host() parsed, but do not let its argument accidentally
    // match ordinary descendants outside a shadow scope.
    if(fn.name=="host") return false;
    if(fn.name=="has") {
      bool any=false;std::vector<const ElementNode*> work;
      if(auto child=node.related('v')) work.push_back(child);
      if(auto next=node.related('+')) work.push_back(next);
      while(!work.empty()&&!any&&budget) {
        auto candidate=work.back();work.pop_back();--budget;
        for(const auto& sub:fn.selectors) if(sub->Match(*candidate,&node,budget,depth+1)) {any=true;break;}
        if(auto next=candidate->related('+')) work.push_back(next);
        if(auto child=candidate->related('v')) work.push_back(child);
      }
      if(!any) return false;continue;
    }
    if(fn.name.starts_with("nth-")) {
      const bool reverse=fn.name.find("last")!=fn.name.npos,typed=fn.name.find("of-type")!=fn.name.npos;
      auto eligible=[&](const ElementNode& item) {
        if(typed&&item.tag!=node.tag) return false;
        if(fn.selectors.empty()) return true;
        for(const auto& sub:fn.selectors) if(sub->Match(item,scope,budget,depth+1)) return true;
        return false;
      };
      if(!eligible(node)) return false;
      int64_t index=1;
      for(auto next=node.related(reverse?'+':'-');next&&budget;next=next->related(reverse?'+':'-')) {
        --budget;if(eligible(*next)) ++index;
      }
      if(!budget) return false;
      int64_t delta=index-fn.b;
      if(fn.a?(delta%fn.a!=0||delta/fn.a<0):delta!=0) return false;
      continue;
    }
    if(fn.name=="lang"||fn.name=="dir") {
      std::string actual;
      for(auto next=&node;next&&budget;next=next->related('>')) {
        --budget;auto it=next->attributes.find(fn.name=="lang"?"lang":"dir");
        if(it!=next->attributes.end()) {actual=CssLower(it->second);break;}
      }
      if(fn.name=="dir"&&actual.empty()) actual="ltr";
      bool any=false;
      for(auto wanted:CssSplit(fn.argument,',')) {
        if(wanted.size()>1&&(wanted[0]=='\''||wanted[0]=='"')) wanted=wanted.substr(1,wanted.size()-2);
        wanted=CssLower(wanted);
        if(actual==wanted||(fn.name=="lang"&&(wanted=="*"?!actual.empty():actual.starts_with(wanted+"-")))) any=true;
      }
      if(!any) return false;continue;
    }
    bool any=false;
    for(const auto& sub:fn.selectors) if(sub->Match(node,scope,budget,depth+1)) {any=true;break;}
    if(fn.name=="not"?any:!any) return false;
  }
  return true;
}
}
