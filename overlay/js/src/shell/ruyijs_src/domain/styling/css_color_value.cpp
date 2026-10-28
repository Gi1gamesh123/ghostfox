#include "domain/styling/css_color_value.h"
#include "domain/styling/css_components.h"
#include "domain/styling/css_numeric_value.h"
#include <algorithm>
namespace ruyijs::domain::styling {
namespace {
bool Channels(const std::string& body,const std::string& name) {
  auto comma=CssSplit(body,',');bool legacy=comma.size()>1;
  auto slash=CssSplit(body,'/');if(slash.size()>2||(legacy&&slash.size()>1)) return false;
  if(slash.size()==2&&slash[1]!="none"&&!CssNumericAllowed(slash[1],CssNumber|CssPercent)) return false;
  std::vector<std::string> channels;
  if(legacy) channels=comma;
  else for(size_t p=0;p<slash[0].size();) {
    p=CssSkip(slash[0],p);if(p==slash[0].size()) break;
    auto end=p;
    while(end<slash[0].size()&&!CssSpace(slash[0][end])) {
      if(slash[0][end]=='(') {auto close=CssClosing(slash[0],end);if(close==slash[0].size()) return false;end=close+1;}
      else ++end;
    }
    channels.push_back(slash[0].substr(p,end-p));p=end;
  }
  if(channels.size()!=3&&!(legacy&&channels.size()==4)) return false;
  for(size_t i=0;i<channels.size();++i) {
    int kinds=CssNumber|CssPercent;
    if((name=="hsl"||name=="hsla"||name=="hwb")&&i==0) kinds=CssNumber|CssAngle;
    else if((name=="hsl"||name=="hsla"||name=="hwb")&&i<3) kinds=CssPercent;
    else if((name=="lch"||name=="oklch")&&i==2) kinds=CssNumber|CssAngle;
    if(channels[i]=="none"&&!legacy) continue;
    if(!CssNumericAllowed(channels[i],kinds)) return false;
  }
  return true;
}
}
bool CssColorSupported(std::string_view text,unsigned depth) {
  if(depth>32) return false;
  auto value=CssLower(CssClean(text));if(value.empty()||!CssValueComponentsValid(value)) return false;
  if(CssColorNameKnown(value)) return true;
  if(value.front()=='#') {
    if(value.size()!=4&&value.size()!=5&&value.size()!=7&&value.size()!=9) return false;
    for(size_t i=1;i<value.size();++i) if(!((value[i]>='0'&&value[i]<='9')||(value[i]>='a'&&value[i]<='f'))) return false;
    return true;
  }
  auto open=value.find('(');if(open==value.npos||CssClosing(value,open)!=value.size()-1) return false;
  auto name=value.substr(0,open),body=value.substr(open+1,value.size()-open-2);
  if(name=="light-dark") {
    auto colors=CssSplit(body,',');return colors.size()==2&&CssColorSupported(colors[0],depth+1)&&CssColorSupported(colors[1],depth+1);
  }
  if(name=="color-mix") {
    auto args=CssSplit(body,',');if(args.size()!=3||args[0].substr(0,3)!="in ") return false;
    const auto spaces=CssSplit("srgb,srgb-linear,display-p3,a98-rgb,prophoto-rgb,rec2020,lab,oklab,xyz,xyz-d50,xyz-d65,hsl,hwb,lch,oklch",',');
    auto space=CssTrim(std::string_view(args[0]).substr(3));
    if(std::find(spaces.begin(),spaces.end(),space)==spaces.end()) return false;
    for(size_t i=1;i<3;++i) {
      auto part=args[i];if(CssColorSupported(part,depth+1)) continue;
      auto split=part.rfind(' ');if(split==part.npos||!CssNumericAllowed(part.substr(split+1),CssPercent,false)||
                                  !CssColorSupported(part.substr(0,split),depth+1)) return false;
    }
    return true;
  }
  if(name=="color") {
    auto split=body.find(' ');if(split==body.npos) return false;
    const auto spaces=CssSplit("srgb,srgb-linear,display-p3,a98-rgb,prophoto-rgb,rec2020,xyz,xyz-d50,xyz-d65",',');
    if(std::find(spaces.begin(),spaces.end(),body.substr(0,split))==spaces.end()) return false;
    return Channels(body.substr(split+1),name);
  }
  if(name=="rgb"||name=="rgba"||name=="hsl"||name=="hsla"||name=="hwb"||name=="lab"||name=="lch"||name=="oklab"||name=="oklch")
    return Channels(body,name);
  return false;
}
}
