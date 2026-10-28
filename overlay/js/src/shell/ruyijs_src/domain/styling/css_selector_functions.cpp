// Compile list/nth/relative pseudos once into the original selector value.
#include "domain/styling/css_selector.h"
#include "domain/styling/css_selector_tokens.h"
#include "domain/styling/css_components.h"
#include <tuple>
namespace ruyijs::domain::styling {
bool ParsedSelector::ParseFunction(const std::string& name,const std::string& body,
                                  SelectorCompound* out,unsigned depth,bool strict,bool inHas) {
  SelectorFunction fn;fn.name=name;fn.argument=CssTrim(body);std::string list=fn.argument;
  if(name=="lang"||name=="dir") {
    if(fn.argument.empty()) return false;
    if(name=="dir") {
      size_t p=0;std::string ident;
      if(!SelectorIdent(fn.argument,&p,&ident)||p!=fn.argument.size()) return false;
    }
    out->function_b++;out->functions.push_back(std::move(fn));return true;
  }
  const bool nth=name=="nth-child"||name=="nth-last-child"||name=="nth-of-type"||name=="nth-last-of-type";
  if(nth) {
    list.clear();if(!SelectorNth(body,&fn.a,&fn.b,&list)) return false;
    if(!list.empty()&&name.find("of-type")!=name.npos) return false;
    out->function_b++;
    if(list.empty()) {out->functions.push_back(std::move(fn));return true;}
  } else if(name!="is"&&name!="where"&&name!="not"&&name!="has"&&name!="host") return false;
  if(name=="host") {
    // :host() is syntactically valid in a document stylesheet too.  Keep the
    // rule so a list such as `html, :host` retains its applicable html branch;
    // tree-scope matching remains owned by the DOM relation input.
    if(fn.argument.empty()) return false;
    auto selector=Parse(fn.argument,depth+1,strict,inHas);
    if(!selector.valid()||selector.has_pseudo_element()) return false;
    fn.selectors.push_back(std::make_shared<ParsedSelector>(std::move(selector)));
    out->functions.push_back(std::move(fn));
    return true;
  }
  if(name=="has"&&inHas) return false;
  const bool forgiving=!strict&&(name=="is"||name=="where");
  int a=0,b=0,c=0;
  for(auto group:CssSplit(list,',')) {
    auto selector=Parse(name=="has"&&!group.empty()?":scope "+group:group,depth+1,strict,inHas||name=="has");
    if(!selector.valid()||selector.has_pseudo_element()) {if(!forgiving) return false;continue;}
    if(name=="has") --selector.b_; // The inserted anchor carries no authored specificity.
    auto current=std::make_tuple(selector.a_,selector.b_,selector.c_);
    if(current>std::make_tuple(a,b,c)) std::tie(a,b,c)=current;
    fn.selectors.push_back(std::make_shared<ParsedSelector>(std::move(selector)));
  }
  if(name!="where") {out->function_a+=a;out->function_b+=b;out->function_c+=c;}
  out->functions.push_back(std::move(fn));return true;
}
}
