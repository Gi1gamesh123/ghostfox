#include "domain/styling/css_condition_value.h"
#include "domain/styling/css_components.h"
#include "domain/styling/css_numeric_value.h"
#include <cmath>
namespace ruyijs::domain::styling {
namespace {
bool Number(std::string_view text,double* value,std::string* unit) {
  auto input=CssTrim(text);size_t end=0;
  if(!CssNumberToken(input,&end,value)) return false;
  *unit=CssLower(input.substr(end));return true;
}
// The prefixed device-pixel-ratio aliases measure the same thing as resolution
// in dppx, so they compare against devicePixelRatio (the environment ratio).
// Firefox has no unprefixed device-pixel-ratio feature, so the names are exact.
bool DevicePixelRatio(const std::string& feature) {
  return feature=="-moz-device-pixel-ratio"||feature=="-webkit-device-pixel-ratio";
}
bool Numeric(std::string text,const std::string& feature,const CssMediaEnvironment& env,double* out) {
  auto slash=CssDelimiter(text,0,"/");
  if(feature=="aspect-ratio"||feature=="device-aspect-ratio") {
    double a,b=1;std::string u,v;
    if(!Number(text.substr(0,slash),&a,&u)||!u.empty()||a<0) return false;
    if(slash<text.size()&&(!Number(text.substr(slash+1),&b,&v)||!v.empty()||b<=0)) return false;
    *out=a/b;return true;
  }
  double n;std::string unit;if(!Number(text,&n,&unit)) return false;
  if(feature=="resolution") {
    if(n<0) return false;
    if(unit=="dpi") n/=96;else if(unit=="dpcm") n*=2.54/96;
    else if(unit!="dppx"&&unit!="x") return false;
  } else if(DevicePixelRatio(feature)) {
    // A device-pixel-ratio is a plain device-pixels-per-CSS-pixel number, so
    // Firefox rejects a unit there.
    if(n<0||!unit.empty()) return false;
  } else if(feature.find("width")!=feature.npos||feature.find("height")!=feature.npos) {
    if(unit=="em"||unit=="rem") n*=16;
    else if(unit=="in") n*=96;else if(unit=="cm") n*=96/2.54;else if(unit=="mm") n*=96/25.4;
    else if(unit=="pt") n*=96/72;else if(unit=="pc") n*=16;
    else if(unit=="vw") n*=env.width/100;else if(unit=="vh") n*=env.height/100;
    else if(unit!="px"&&!unit.empty()) return false;
    else if(unit.empty()&&n!=0) return false;
  } else if(!unit.empty()||std::trunc(n)!=n) return false;
  *out=n;return true;
}
bool FeatureNumber(const std::string& name,const CssMediaEnvironment& env,double* value) {
  if(name=="width") *value=env.width;else if(name=="height") *value=env.height;
  else if(name=="device-width") *value=env.deviceWidth;else if(name=="device-height") *value=env.deviceHeight;
  else if(name=="aspect-ratio") *value=env.height?env.width/env.height:0;
  else if(name=="device-aspect-ratio") *value=env.deviceHeight?env.deviceWidth/env.deviceHeight:0;
  else if(name=="resolution"||DevicePixelRatio(name)) *value=env.ratio;
  else if(name=="color") *value=env.color;else if(name=="color-index"||name=="monochrome") *value=0;
  else return false;
  return true;
}
// A configured gamut or dynamic range also satisfies every narrower value.
bool ProfileMatches(const std::string& name,const std::string& have,const std::string& want) {
  auto rank=[](const std::string& v) {return v=="srgb"||v=="standard"?0:v=="p3"||v=="high"?1:v=="rec2020"?2:-1;};
  if(name.find("gamut")!=name.npos||name.find("dynamic-range")!=name.npos)
    return rank(want)>=0&&rank(want)<=rank(have);
  return have==want;
}
bool Compare(double a,double b,const std::string& op) {
  return op=="<"?a<b:op=="<="?a<=b:op==">"?a>b:op==">="?a>=b:a==b;
}
CssConditionResult Range(const std::string& value,const CssMediaEnvironment& env) {
  std::vector<std::string> operands,operators;
  for(size_t start=0;start<value.size();) {
    auto p=CssDelimiter(value,start,"<>=");operands.push_back(CssTrim(std::string_view(value).substr(start,p-start)));
    if(p==value.size()) break;
    std::string op(1,value[p++]);if(p<value.size()&&value[p]=='=') {op+='=';++p;}
    if(op=="==") return {true,false,false};
    operators.push_back(op);start=p;
  }
  if(operators.empty()||operators.size()>2||operands.size()!=operators.size()+1) return {};
  if(operators.size()==2) {
    if(operators[0][0]=='='||operators[0][0]!=operators[1][0]) return {true,false,false};
    double n,a,b;if(!FeatureNumber(operands[1],env,&n)||!Numeric(operands[0],operands[1],env,&a)||
                     !Numeric(operands[2],operands[1],env,&b)) return {true,false,false};
    return {true,Compare(a,n,operators[0])&&Compare(n,b,operators[1])};
  }
  double actual,wanted;
  bool inverse=!FeatureNumber(operands[0],env,&actual);
  auto feature=inverse?operands[1]:operands[0],number=inverse?operands[0]:operands[1];
  if((inverse&&!FeatureNumber(feature,env,&actual))||!Numeric(number,feature,env,&wanted)) return {true,false,false};
  return {true,Compare(inverse?wanted:actual,inverse?actual:wanted,operators[0])};
}
}
CssConditionResult CssMediaFeature(std::string_view text,const CssMediaEnvironment& env) {
  const auto value=CssLower(CssTrim(text));auto colon=CssDelimiter(value,0,":");
  if(colon==value.size()) {
    if(CssDelimiter(value,0,"<>=")<value.size()) return Range(value,env);
    double n;if(FeatureNumber(value,env,&n)) return {true,n!=0};
    auto set=env.profile.find(value);
    if(set!=env.profile.end()) return {true,set->second!="none"&&set->second!="no-preference"};
    if(value=="hover"||value=="any-hover") return {true,!env.touch};
    if(value=="pointer"||value=="any-pointer"||value=="prefers-color-scheme") return {true,true};
    if(value=="prefers-reduced-motion"||value=="prefers-contrast"||value=="forced-colors")
      return {true,false};
    return value.empty()?CssConditionResult{}:CssConditionResult{true,false,false};
  }
  auto name=CssTrim(std::string_view(value).substr(0,colon)),expected=CssTrim(std::string_view(value).substr(colon+1));
  std::string relation;
  bool prefixed=false;
  if(name.substr(0,4)=="min-"||name.substr(0,4)=="max-") {relation=name.substr(0,3);prefixed=true;name=name.substr(4);}
  // Firefox keeps the Chromium aliases as distinct feature names whose min-/max-
  // relation sits inside the prefix: -webkit-min-device-pixel-ratio is
  // min-resolution in dppx, and -webkit-max-device-pixel-ratio is max-resolution.
  else if(name.substr(0,12)=="-webkit-min-"||name.substr(0,12)=="-webkit-max-") {
    relation=name.substr(8,3);name="-webkit-"+name.substr(12);
  }
  // That alias exists only in its own prefixed spelling, so a second relation
  // prefix in front of it (min--webkit-device-pixel-ratio) is not a feature.
  if(prefixed&&name=="-webkit-device-pixel-ratio") return {true,false,false};
  double actual,wanted;
  if(FeatureNumber(name,env,&actual)) {
    if(!Numeric(expected,name,env,&wanted)) return {true,false,false};
    return {true,relation=="min"?actual>=wanted:relation=="max"?actual<=wanted:actual==wanted};
  }
  if(!relation.empty()) return {true,false,false};
  auto set=env.profile.find(name);
  if(set!=env.profile.end()) return {true,ProfileMatches(name,set->second,expected)};
  std::string chosen;
  if(name=="orientation") chosen=env.width>=env.height?"landscape":"portrait";
  else if(name=="hover"||name=="any-hover") chosen=env.touch?"none":"hover";
  else if(name=="pointer"||name=="any-pointer") chosen=env.touch?"coarse":"fine";
  else if(name=="prefers-color-scheme") chosen="light";
  else if(name=="prefers-reduced-motion"||name=="prefers-contrast") chosen="no-preference";
  else if(name=="forced-colors") chosen="none";
  else if(name=="display-mode") chosen="browser";else if(name=="update") chosen="fast";
  else if(name=="overflow-block"||name=="overflow-inline") chosen="scroll";
  else if(name=="scripting") chosen="enabled";else if(name=="dynamic-range"||name=="video-dynamic-range") chosen="standard";
  else if(name=="color-gamut"||name=="video-color-gamut") chosen="srgb";
  // A feature this engine cannot evaluate is left unknown, so the query never
  // matches and the normalized media text keeps the author's feature name.
  // Firefox behaves the same for features it does not implement, such as
  // inverted-colors and prefers-reduced-transparency: the query is "not all".
  return {true,!chosen.empty()&&chosen==expected,!chosen.empty()};
}
}
