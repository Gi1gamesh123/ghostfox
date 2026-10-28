#include "domain/styling/box_layout_internal.h"
#include "domain/styling/css_components.h"
#include "domain/styling/css_numeric_value.h"

namespace ruyijs::domain::styling {
namespace {
using Matrix = std::array<double, 6>;
Matrix Multiply(const Matrix& a, const Matrix& b) {
  return {a[0]*b[0]+a[2]*b[1], a[1]*b[0]+a[3]*b[1], a[0]*b[2]+a[2]*b[3],
          a[1]*b[2]+a[3]*b[3], a[0]*b[4]+a[2]*b[5]+a[4], a[1]*b[4]+a[3]*b[5]+a[5]};
}
double Resolve(const BoxStyle& style, const std::string& text, double percent, double fallback) {
  const auto value = CssNumericResolve(text, style.font_size, style.root_font_size, percent);
  return value.kind ? value.number : fallback;
}
double Angle(const std::string& text) {
  const auto number = CssNumericParse(text);
  const double pi = std::acos(-1.0);
  auto ends = [&](const char* suffix) { const std::string s(suffix); return text.size() >= s.size() && text.compare(text.size()-s.size(),s.size(),s)==0; };
  if (ends("rad")) return ends("grad") ? number.number*pi/200 : number.number;
  return ends("turn") ? number.number*pi*2 : number.number*pi/180;
}
Matrix Transform(const BoxStyle& style, const BoxRect& box) {
  Matrix result{1,0,0,1,0,0};
  const auto text = CssLower(style.Get("transform"));
  for (size_t p = 0; p < text.size();) {
    p = CssSkip(text,p);
    const auto open = text.find('(',p);
    if (open == text.npos) break;
    const auto end = CssClosing(text,open);
    if (end >= text.size()) break;
    const auto name = CssTrim(text.substr(p,open-p));
    const auto args = CssSplit(text.substr(open+1,end-open-1), ',');
    auto get = [&](size_t i, double percent, double fallback) {
      return i < args.size() ? Resolve(style,args[i],percent,fallback) : fallback;
    };
    Matrix next{1,0,0,1,0,0};
    if (name == "matrix" && args.size() == 6) for (unsigned i=0;i<6;++i) next[i]=get(i,0,next[i]);
    else if (name == "translate" || name == "translatex") { next[4]=get(0,box[2],0); next[5]=get(1,box[3],0); }
    else if (name == "translatey") next[5]=get(0,box[3],0);
    else if (name == "scale") { next[0]=get(0,0,1); next[3]=get(1,0,next[0]); }
    else if (name == "scalex") next[0]=get(0,0,1);
    else if (name == "scaley") next[3]=get(0,0,1);
    else if (name == "rotate" && !args.empty()) {
      const double angle=Angle(args[0]); next={std::cos(angle),std::sin(angle),-std::sin(angle),std::cos(angle),0,0};
    } else if (name == "skewx" && !args.empty()) next[2]=std::tan(Angle(args[0]));
    else if (name == "skewy" && !args.empty()) next[1]=std::tan(Angle(args[0]));
    result=Multiply(result,next); p=end+1;
  }
  double ox=box[2]/2, oy=box[3]/2;
  const auto origin=style.Get("transform-origin");
  const auto parts=CssSplit(origin,' ');
  auto offset=[&](const std::string& part,double basis) {
    if(part=="left"||part=="top") return 0.0;
    if(part=="right"||part=="bottom") return basis;
    return part=="center"?basis/2:Resolve(style,part,basis,basis/2);
  };
  if(!origin.empty()&&!parts.empty()) { ox=offset(parts[0],box[2]); if(parts.size()>1) oy=offset(parts[1],box[3]); }
  ox+=box[0]; oy+=box[1];
  return Multiply(Multiply({1,0,0,1,ox,oy},result),{1,0,0,1,-ox,-oy});
}
}
BoxRect ProjectBoxRect(const std::vector<BoxNode>& nodes, const std::vector<BoxLayout>& boxes,
                       const BoxViewport& viewport, size_t target) {
  Matrix matrix{1,0,0,1,0,0};
  for(size_t i=target;i!=kNoBox;i=nodes[i].parent)
    matrix=Multiply(Transform(nodes[i].style,boxes[i].border),matrix);
  const auto& box=boxes[target].border;
  double xs[4],ys[4];
  for(unsigned i=0;i<4;++i) {
    const double x=box[0]+(i&1?box[2]:0), y=box[1]+(i&2?box[3]:0);
    xs[i]=matrix[0]*x+matrix[2]*y+matrix[4]-viewport.scroll_x;
    ys[i]=matrix[1]*x+matrix[3]*y+matrix[5]-viewport.scroll_y;
  }
  const auto x=std::minmax_element(xs,xs+4),y=std::minmax_element(ys,ys+4);
  // nsLayoutUtils clips transformed bounds to the coordinate range. A span
  // wider than nscoord_MAX is centered and shrunk, rather than overflowing.
  auto saturated=[](double low, double high) {
    const double limit=kBoxCoordMax/60;
    low=std::clamp(low,-limit,limit);high=std::clamp(high,-limit,limit);
    if(high-low>limit) {low+=(high-low-limit)*.5;high=low+limit;}
    return std::array<double,2>{low,high};
  };
  const auto horizontal=saturated(*x.first,*x.second),vertical=saturated(*y.first,*y.second);
  // Firefox DOMRect::SetLayoutRect quantizes app-unit edges to 1/65536 CSS px,
  // then stores each component through SetRect(float, float, float, float).
  auto edge=[](double value) { return std::floor(BoxRound(value)*65536+.5)/65536; };
  const double left=edge(horizontal[0]),top=edge(vertical[0]);
  return {double(float(left)),double(float(top)),double(float(edge(horizontal[1])-left)),
          double(float(edge(vertical[1])-top))};
}
}
