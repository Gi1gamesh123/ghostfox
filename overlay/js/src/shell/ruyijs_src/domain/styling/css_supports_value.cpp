#include "domain/styling/css_condition_value.h"
#include "domain/styling/css_components.h"
#include "domain/styling/css_color_value.h"
#include "domain/styling/css_numeric_value.h"
#include "domain/styling/stylesheet.h"
#include "domain/styling/css_box_value.h"
#include <algorithm>
namespace ruyijs::domain::styling {
namespace {
bool Keyword(const std::string& value,const char* list) {
  for(const auto& word:CssSplit(list,',')) if(word==value) return true;
  return false;
}
bool Identifier(const std::string& text) {
  if(text.empty()||(text[0]>='0'&&text[0]<='9')) return false;
  for(unsigned char c:text) if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
       (c>='0'&&c<='9')||c=='-'||c=='_'||c>=128)) return false;
  return true;
}
bool Display(const std::string& value) {
  if(Keyword(value,"none,contents,block,inline,inline-block,flow-root,flex,inline-flex,grid,inline-grid,table,inline-table,table-row,table-cell,table-caption,table-column,table-column-group,table-row-group,table-header-group,table-footer-group,list-item,ruby,ruby-base,ruby-text,ruby-base-container,ruby-text-container")) return true;
  std::vector<std::string> words;
  for(auto word:CssSplit(value,' ')) if(!word.empty()) words.push_back(word);
  bool outside=false,inside=false,list=false;
  for(const auto& word:words) {
    if(Keyword(word,"block,inline,run-in")) {if(outside) return false;outside=true;}
    else if(Keyword(word,"flow,flow-root,table,flex,grid,ruby")) {if(inside) return false;inside=true;}
    else if(word=="list-item") {if(list) return false;list=true;}
    else return false;
  }
  if(list) for(const auto& word:words) if(Keyword(word,"flex,grid,table,ruby,run-in")) return false;
  return words.size()>1;
}
bool Box(const std::string& value,bool negative,bool automatic,bool many) {
  std::vector<std::string> values;
  for(size_t p=0;p<value.size();) {
    p=CssSkip(value,p);if(p==value.size()) break;auto end=p;
    while(end<value.size()&&!CssSpace(value[end])) {
      if(value[end]=='(') {auto close=CssClosing(value,end);if(close==value.size()) return false;end=close+1;}
      else ++end;
    }
    values.push_back(value.substr(p,end-p));p=end;
  }
  if(values.empty()||values.size()>(many?4u:1u)) return false;
  for(const auto& part:values) if(!(automatic&&part=="auto")&&
      !CssNumericAllowed(part,CssLength|CssPercent,negative)) return false;
  return true;
}
}
bool CssSupportsProperty(const std::string& property,const std::string& input) {
  const auto name=CssLower(property),value=CssClean(input),lower=CssLower(value);
  if(!Identifier(property)||!CssValueComponentsValid(input,name.substr(0,2)=="--")) return false;
  if(name.substr(0,2)=="--") return name.size()>2;
  if(name=="font") {
    std::vector<CssDeclarationValue> expanded;
    return CssFontExpand(input,false,&expanded);
  }
  if(CssBoxShorthand(name)) {
    std::vector<CssDeclarationValue> expanded;
    return CssBoxExpand(name,input,false,&expanded);
  }
  if(value.empty()||!StyleResolutionService::initial_values().count(name)) return false;
  if(Keyword(lower,"initial,inherit,unset,revert,revert-layer")) return true;
  if(lower.find("var(")!=lower.npos||lower.find("env(")!=lower.npos) return CssVariableReferenceValid(lower);
  if(name=="display") return Display(lower);
  if(name=="box-sizing") return Keyword(lower,"content-box,border-box");
  if(name=="color"||name.find("color")!=name.npos) return CssColorSupported(lower);
  if(name=="line-height") return lower=="normal"||CssNumericAllowed(lower,CssNumber|CssLength|CssPercent,false);
  if(name=="width"||name=="height"||name.find("-width")!=name.npos||name.find("-height")!=name.npos) {
    if(name.find("border-")==0&&Keyword(lower,"thin,medium,thick")) return true;
    return Keyword(lower,"auto,none,min-content,max-content,fit-content,stretch")||Box(lower,false,false,false);
  }
  if(name.find("margin")==0) return Box(lower,true,true,name=="margin");
  if(name.find("padding")==0) return Box(lower,false,false,name=="padding");
  if(Keyword(name,"left,right,top,bottom")) return Box(lower,true,true,false);
  if(name=="opacity") return CssNumericAllowed(lower,CssNumber|CssPercent);
  if(name=="z-index") return lower=="auto"||CssNumericAllowed(lower,CssNumber,true,true);
  if(name=="font-weight") {
    if(Keyword(lower,"normal,bold,bolder,lighter")) return true;
    auto n=CssNumericParse(lower);return n.kind==CssNumber&&(n.expression||(n.number>=1&&n.number<=1000));
  }
  if(name=="font-size") return Keyword(lower,"xx-small,x-small,small,medium,large,x-large,xx-large,xxx-large,smaller,larger")||Box(lower,false,false,false);
  if(name=="font-style") return Keyword(lower,"normal,italic,oblique");
  if(name=="letter-spacing"||name=="word-spacing") return lower=="normal"||CssNumericAllowed(lower,CssLength,true);
  if(name=="font-family") {
    for(auto part:CssSplit(value,',')) {
      if(part.size()>=2&&(part.front()=='\''||part.front()=='"')&&part.back()==part.front()) continue;
      for(auto word:CssSplit(part,' ')) if(!Identifier(word)) return false;
    }
    return true;
  }
  if(name=="flex-grow"||name=="flex-shrink") return CssNumericAllowed(lower,CssNumber,false);
  if(name=="flex-basis") return Keyword(lower,"auto,content,min-content,max-content,fit-content")||Box(lower,false,false,false);
  if(name=="flex-direction") return Keyword(lower,"row,row-reverse,column,column-reverse");
  if(name=="flex-wrap") return Keyword(lower,"nowrap,wrap,wrap-reverse");
  if(name=="order") return CssNumericAllowed(lower,CssNumber,true,true);
  if(name=="row-gap"||name=="column-gap") return lower=="normal"||Box(lower,false,false,false);
  if(name=="align-items"||name=="align-self"||name=="align-content"||name=="justify-content") {
    const bool self=name=="align-self",items=name=="align-items";
    if(self&&lower=="auto") return true;
    if(Keyword(lower,"normal,stretch,center,start,end,flex-start,flex-end,baseline,first baseline,last baseline,safe center,unsafe center,safe end,unsafe end,safe flex-end,unsafe flex-end,safe start,unsafe start,safe flex-start,unsafe flex-start")) return true;
    if((self||items)&&Keyword(lower,"self-start,self-end,safe self-start,safe self-end,unsafe self-start,unsafe self-end")) return true;
    if(!self&&!items&&Keyword(lower,"space-between,space-around,space-evenly")) return true;
    return name=="justify-content"&&Keyword(lower,"left,right,safe left,safe right,unsafe left,unsafe right");
  }
  if(name=="position") return Keyword(lower,"static,relative,absolute,fixed,sticky");
  if(name=="visibility") return Keyword(lower,"visible,hidden,collapse");
  if(name=="overflow-x"||name=="overflow-y") return Keyword(lower,"visible,hidden,clip,scroll,auto");
  if(name=="white-space") return Keyword(lower,"normal,pre,nowrap,pre-wrap,pre-line,break-spaces");
  if(name=="text-align") return Keyword(lower,"start,end,left,right,center,justify,match-parent");
  if(name=="float") return Keyword(lower,"none,left,right,inline-start,inline-end");
  if(name=="clear") return Keyword(lower,"none,left,right,both,inline-start,inline-end");
  if(name.find("-style")!=name.npos) return Keyword(lower,"none,hidden,dotted,dashed,solid,double,groove,ridge,inset,outset");
  if(name=="background-image") return lower=="none";
  if(name=="cursor") return Keyword(lower,"auto,default,none,context-menu,help,pointer,progress,wait,cell,crosshair,text,vertical-text,alias,copy,move,no-drop,not-allowed,grab,grabbing,all-scroll,col-resize,row-resize,n-resize,e-resize,s-resize,w-resize,ne-resize,nw-resize,se-resize,sw-resize,ew-resize,ns-resize,nesw-resize,nwse-resize,zoom-in,zoom-out");
  // Other property grammars are pending; arbitrary tokens do not imply support.
  return false;
}
bool CssSupportsCondition(const std::string& text) {
  auto answer=CssBooleanCondition(text,[](std::string_view atom)->CssConditionResult {
    auto value=CssClean(atom),lower=CssLower(value);auto open=value.find('(');
    if(open!=value.npos&&CssDelimiter(value,0,":")==value.size()&&CssClosing(value,open)==value.size()-1) {
      auto name=lower.substr(0,open),body=value.substr(open+1,value.size()-open-2);
      if(name=="selector") return {true,CssSelectorSupported(body)};
      if(name=="font-tech") return {true,Keyword(CssLower(body),"features-opentype,features-aat,features-graphite,color-colrv0,color-colrv1,color-svg,color-sbix,color-cbdt,variations,palettes,incremental")};
      if(name=="font-format") return {true,Keyword(CssLower(body),"collection,embedded-opentype,opentype,svg,truetype,woff,woff2")};
      return {true,false};
    }
    const auto colon=CssDelimiter(value,0,":");
    if(colon<value.size()) {
      auto input=value.substr(colon+1),name=CssTrim(value.substr(0,colon));
      auto bang=CssDelimiter(input,0,"!");
      if(bang<input.size()&&CssLower(CssTrim(std::string_view(input).substr(bang+1)))=="important") input.resize(bang);
      return {true,CssSupportsProperty(name,input)};
    }
    if(value.find('(')!=value.npos||lower.substr(0,4)=="not ") return {};
    return {true,false};
  });
  return answer.valid&&answer.value;
}
}
