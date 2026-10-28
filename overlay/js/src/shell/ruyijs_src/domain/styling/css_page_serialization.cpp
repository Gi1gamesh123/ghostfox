#include "domain/styling/css_page_value.h"
#include "domain/styling/css_box_value.h"
#include "domain/styling/css_color_value.h"
#include <algorithm>
#include <set>
namespace ruyijs::domain::styling {
std::string CssPageSerialize(const std::vector<CssDeclarationValue>& rows,
                             const std::string& name,bool* important) {
  if(important) *important=false;
  const auto names=CssPageLonghands(name);
  if(names.empty()) return {};
  std::vector<std::string> values;bool flag=false,pending=false;
  for(const auto& key:names) {
    const auto it=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.name==key;});
    if(it==rows.end()||(!values.empty()&&it->important!=flag)) return {};
    if(!it->pending_shorthand.empty()) {
      if(it->pending_shorthand!=name) return {};
      pending=true;
    } else if(CssVariableReferenceValid(it->value)) return {};
    flag=it->important;values.push_back(it->value);
  }
  const bool equal=std::all_of(values.begin(),values.end(),[&](const auto& v){return v==values[0];});
  if(pending&&!equal) return {};
  if(!equal&&std::any_of(values.begin(),values.end(),[](const auto& v){return CssPageWide(v);})) return {};
  std::string result;
  if(equal) result=values[0];
  else if(names.size()==4) result=CssBoxSerialize(rows,name);
  else result=values[0]+" "+values[1];
  if(important&&!result.empty()) *important=flag;
  return result;
}
std::string CssPageLookup(const std::vector<CssDeclarationValue>& rows,const std::string& name,bool* important) {
  *important=false;
  // Internal logical longhands are readable/removable although page input
  // descriptor validation rejects assigning them directly.
  if(!CssPageLonghands(name).empty()) return CssPageSerialize(rows,name,important);
  for(const auto& row:rows) if(row.name==name) {
    *important=row.important;return row.pending_shorthand.empty()?row.value:"";
  }
  return {};
}
std::string CssPageText(const std::vector<CssDeclarationValue>& rows) {
  std::string result;std::set<std::string> emitted;
  for(const auto& row:rows) {
    if(emitted.contains(row.name)) continue;
    auto name=row.name,value=row.value;bool important=row.important;
    for(auto shorthand:{"margin","margin-block","margin-inline","padding-block","padding-inline"}) {
      const auto names=CssPageLonghands(shorthand);
      if(std::find(names.begin(),names.end(),row.name)==names.end()) continue;
      bool flag=false;auto combined=CssPageSerialize(rows,shorthand,&flag);
      if(!combined.empty()) {
        name=shorthand;value=std::move(combined);important=flag;
        emitted.insert(names.begin(),names.end());
      }
      break;
    }
    if(!row.pending_shorthand.empty()&&name==row.name) value.clear();
    if(!result.empty()) result+=' ';
    result+=name+": "+value+(important?" !important":"")+";";
  }
  return result;
}
std::string CssPageSet(std::vector<CssDeclarationValue>& rows,const std::string& name,const std::string& value,
                        bool important,bool remove) {
  bool oldFlag=false;const auto old=CssPageLookup(rows,name,&oldFlag);
  if(!remove&&!value.empty()&&!CssPageProperty(name)) return old;
  std::vector<CssDeclarationValue> expanded;
  if(!remove&&!value.empty()&&!CssPageExpand(name,value,important,&expanded)) return old;
  auto names=CssPageLonghands(name);
  if(names.empty()) names.push_back(name);
  if(remove||value.empty()) {
    rows.erase(std::remove_if(rows.begin(),rows.end(),[&](const auto& row){
      return std::find(names.begin(),names.end(),row.name)!=names.end();
    }),rows.end());
  } else for(auto& replacement:expanded) {
    const auto found=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.name==replacement.name;});
    if(found==rows.end()) rows.push_back(std::move(replacement));else *found=std::move(replacement);
  }
  return old;
}
}
