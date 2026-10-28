#include "domain/styling/css_numeric_value.h"
#include "domain/styling/css_components.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>
namespace ruyijs::domain::styling {
namespace {
bool Digit(char c) {return c>='0'&&c<='9';}
bool UnitIn(const std::string& unit,const char* list) {
  std::string_view remaining(list);
  while(!remaining.empty()) {
    const auto end=remaining.find(',');
    if(remaining.substr(0,end)==unit) return true;
    if(end==remaining.npos) break;
    remaining.remove_prefix(end+1);
  }
  return false;
}
int UnitKind(const std::string& unit) {
  if(unit.empty()) return CssNumber;
  if(unit=="%") return CssPercent;
  if(UnitIn(unit,"px,em,rem,ex,rex,ch,rch,cap,rcap,ic,ric,lh,rlh,in,cm,mm,q,pt,pc,vw,vh,vi,vb,vmin,vmax,svw,svh,svi,svb,svmin,svmax,lvw,lvh,lvi,lvb,lvmin,lvmax,dvw,dvh,dvi,dvb,dvmin,dvmax,cqw,cqh,cqi,cqb,cqmin,cqmax")) return CssLength;
  if(UnitIn(unit,"deg,rad,grad,turn")) return CssAngle;
  if(unit=="s"||unit=="ms") return CssTime;
  if(UnitIn(unit,"dpi,dpcm,dppx,x")) return CssResolution;
  return CssInvalid;
}
int AddKind(int a,int b) {
  if(a==b) return a;
  if((a|b)==(CssLength|CssPercent)) return a|b;
  return CssInvalid;
}
class NumericParser {
 public:
  std::string text;size_t p=0;unsigned depth=0;
  bool resolve=false;
  double em=16,rem=16,percent=16;
  explicit NumericParser(std::string_view v):text(CssLower(CssClean(v))) {}
  void Space() {p=CssSkip(text,p);}
  CssNumericValue Sum() {
    auto a=Product();
    while(a.kind) {
      size_t end=p;Space();
      if(p==text.size()||(text[p]!='+'&&text[p]!='-')) break;
      // CSS calc sum operators require whitespace on both sides.
      if(p==end||p+1==text.size()||!CssSpace(text[p+1])) return {};
      char op=text[p++];auto b=Product();int kind=AddKind(a.kind,b.kind);
      if(!b.kind||!kind) return {};
      a={kind,op=='+'?a.number+b.number:a.number-b.number,true};
    }
    return a;
  }
  CssNumericValue Product() {
    auto a=Primary();
    while(a.kind) {
      size_t end=p;Space();if(p==text.size()||(text[p]!='*'&&text[p]!='/')) {p=end;break;}
      char op=text[p++];auto b=Primary();if(!b.kind) return {};
      int kind=op=='*'?(a.kind==CssNumber?b.kind:b.kind==CssNumber?a.kind:0):
                           (b.kind==CssNumber?a.kind:a.kind==b.kind?CssNumber:0);
      if(!kind) return {};
      a={kind,op=='*'?a.number*b.number:a.number/b.number,true};
    }
    return a;
  }
  CssNumericValue Primary() {
    Space();if(p==text.size()||depth>=64) return {};
    ++depth;auto result=Atom();--depth;return result;
  }
  CssNumericValue Atom() {
    size_t start=p;
    if(text[p]=='(') {
      ++p;auto v=Sum();Space();if(p==text.size()||text[p++]!=')') return {};
      v.expression=true;return v;
    }
    double number;size_t end=p;
    if(CssNumberToken(text,&end,&number)) {
      p=end;
      while(p<text.size()&&((text[p]>='a'&&text[p]<='z')||text[p]=='%')) ++p;
      const auto unit=text.substr(end,p-end);int kind=UnitKind(unit);
      if(resolve&&(kind==CssLength||kind==CssPercent)) {
        double factor=0;
        if(unit=="px") factor=1;
        else if(unit=="em") factor=em;
        else if(unit=="rem") factor=rem;
        else if(unit=="%") factor=percent/100;
        else if(unit=="in") factor=96;
        else if(unit=="cm") factor=96/2.54;
        else if(unit=="mm") factor=96/25.4;
        else if(unit=="q") factor=96/101.6;
        else if(unit=="pt") factor=96.0/72;
        else if(unit=="pc") factor=16;
        else return {};
        return {CssLength,number*factor,false};
      }
      return {kind,number,false};
    }
    while(p<text.size()&&((text[p]>='a'&&text[p]<='z')||text[p]=='-')) ++p;
    auto name=text.substr(start,p-start);
    if(p==text.size()||text[p++]!='(') return {};
    if(name!="calc"&&name!="min"&&name!="max"&&name!="clamp") return {};
    auto v=Sum();unsigned args=1;Space();
    double minimum=v.number,center=0;
    while(p<text.size()&&text[p]==',') {
      ++p;auto other=Sum();int kind=AddKind(v.kind,other.kind);
      if(!kind) return {};
      if(resolve) {
        if(name=="min") v.number=std::min(v.number,other.number);
        else if(name=="max") v.number=std::max(v.number,other.number);
        else if(name=="clamp") {
          if(args==1) center=other.number;
          else v.number=std::max(minimum,std::min(center,other.number));
        }
      }
      v.kind=kind;++args;Space();
    }
    if(!v.kind||p==text.size()||text[p++]!=')'||(name=="calc"&&args!=1)||(name=="clamp"&&args!=3)) return {};
    v.expression=true;return v;
  }
};
}
bool CssNumberToken(std::string_view text,size_t* cursor,double* number) {
  size_t p=*cursor,start=p;
  if(p<text.size()&&(text[p]=='+'||text[p]=='-')) ++p;
  size_t digits=p;while(p<text.size()&&Digit(text[p])) ++p;bool any=p>digits;
  if(p+1<text.size()&&text[p]=='.'&&Digit(text[p+1])) {
    p+=2;while(p<text.size()&&Digit(text[p])) ++p;any=true;
  }
  if(!any) return false;
  if(p<text.size()&&(text[p]=='e'||text[p]=='E')) {
    size_t q=p+1;if(q<text.size()&&(text[q]=='+'||text[q]=='-')) ++q;
    size_t e=q;while(q<text.size()&&Digit(text[q])) ++q;if(q>e) p=q;
  }
  *number=std::strtod(std::string(text.substr(start,p-start)).c_str(),nullptr);
  if(!std::isfinite(*number)) return false;
  *cursor=p;return true;
}
CssNumericValue CssNumericParse(std::string_view text) {
  NumericParser parser(text);auto result=parser.Primary();parser.Space();
  if(parser.p!=parser.text.size()) return {};
  return result;
}
CssNumericValue CssNumericResolve(std::string_view text,double em,double rem,double percent) {
  NumericParser parser(text);parser.resolve=true;parser.em=em;parser.rem=rem;parser.percent=percent;
  auto result=parser.Primary();parser.Space();
  if(parser.p!=parser.text.size()||!std::isfinite(result.number)) return {};
  return result;
}
bool CssNumericAllowed(std::string_view text,int kinds,bool negative,bool integer) {
  auto value=CssNumericParse(text);
  if(!value.kind) return false;
  if(value.kind==CssNumber&&value.number==0&&!integer&&(kinds&CssLength)) return true;
  return !(value.kind&~kinds)&&(negative||value.expression||value.number>=0)&&
      (!integer||value.expression||std::trunc(value.number)==value.number);
}
}
