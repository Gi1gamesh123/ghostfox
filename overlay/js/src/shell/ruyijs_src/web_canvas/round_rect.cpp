// RoundRect records a bounded polygon through the existing path store (D69).
#include "web_canvas/internal.h"
#include <algorithm>
#include "js/ErrorReport.h"
#include "js/ForOfIterator.h"
#include "js/Symbol.h"

namespace ruyijs::web_canvas {
namespace {
struct Radius { double x=0, y=0; };
bool ReadRadius(JSContext* cx,JS::HandleValue value,Radius* out) {
  if (value.isObject()) {
    JS::RootedObject object(cx,&value.toObject());
    JS::RootedValue x(cx),y(cx);
    if (!JS_GetProperty(cx,object,"x",&x) || !JS_GetProperty(cx,object,"y",&y)) return false;
    return (x.isUndefined() || JS::ToNumber(cx,x,&out->x)) &&
           (y.isUndefined() || JS::ToNumber(cx,y,&out->y));
  }
  if (value.isNullOrUndefined()) return true;
  if (!JS::ToNumber(cx,value,&out->x)) return false;
  out->y=out->x;return true;
}
bool Radii(JSContext* cx,JS::HandleValue value,Radius (&r)[4],bool* usable) {
  std::vector<Radius> values;
  bool sequence=false;
  if (value.isObject()) {
    JS::RootedObject object(cx,&value.toObject());
    JS::RootedId key(cx,JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx,JS::SymbolCode::iterator)));
    JS::RootedValue iter(cx);
    if (!JS_GetPropertyById(cx,object,key,&iter)) return false;
    sequence=!iter.isNullOrUndefined();
  }
  if (sequence) {
    JS::ForOfIterator iter(cx);
    if (!iter.init(value)) return false;
    for (;;) {
      bool done;JS::RootedValue item(cx);
      if (!iter.next(&item,&done)) return false;
      if (done) break;
      if (values.size()==4) return WebKit::FailKind(cx,"roundRect needs one to four radii",JSEXN_RANGEERR);
      Radius radius;
      if (!ReadRadius(cx,item,&radius)) return false;
      values.push_back(radius);
    }
  } else {
    Radius radius;
    if (!ReadRadius(cx,value,&radius)) return false;
    values.push_back(radius);
  }
  if (values.empty()) return WebKit::FailKind(cx,"roundRect needs one to four radii",JSEXN_RANGEERR);
  *usable=true;
  for (const auto& v:values) {
    if (!std::isfinite(v.x) || !std::isfinite(v.y)) *usable=false;
    if (v.x<0 || v.y<0) return WebKit::FailKind(cx,"Negative roundRect radius",JSEXN_RANGEERR);
  }
  r[0]=values[0];r[1]=values.size()>1?values[1]:r[0];
  r[2]=values.size()>2?values[2]:r[0];r[3]=values.size()>3?values[3]:r[1];
  return true;
}
}
bool RoundRect(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  const bool path=detail::ContextSlot(args)==100;
  JS::RootedObject self(cx);
  if (path ? !WebKit::Check(cx,args.thisv(),&kPath2DClass,&self,"Illegal invocation")
           : !detail::ContextReceiver(cx,args,&self)) return false;
  if (argc<4) return WebKit::Fail(cx,"roundRect requires four coordinates");
  double rect[4];bool valid;
  // Convert all required arguments even when an earlier value is non-finite.
  for (int i=0;i<4;++i) if (!JS::ToNumber(cx,args[i],&rect[i])) return false;
  Radius r[4];
  if (!Radii(cx,args.get(4),r,&valid)) return false;
  args.rval().setUndefined();
  for (double n:rect) valid &= std::isfinite(n);
  if (!valid) return true;
  double x=rect[0],y=rect[1],w=rect[2],h=rect[3];
  const bool reverse=(w<0)!=(h<0);
  if (w<0) {x+=w;w=-w;std::swap(r[0],r[1]);std::swap(r[2],r[3]);}
  if (h<0) {y+=h;h=-h;std::swap(r[0],r[3]);std::swap(r[1],r[2]);}
  double scale=1;
  const double edges[]={r[0].x+r[1].x,r[3].x+r[2].x,r[0].y+r[3].y,r[1].y+r[2].y};
  for (int i=0;i<4;++i) if (edges[i]>0) scale=std::min(scale,(i<2?w:h)/edges[i]);
  for (auto& v:r) {v.x*=scale;v.y*=scale;}
  std::vector<Radius> points;
  constexpr double pi=3.14159265358979323846;
  for (int corner=0;corner<4;++corner) {
    const double cx0=corner==0 || corner==3?x+r[corner].x:x+w-r[corner].x;
    const double cy0=corner<2?y+r[corner].y:y+h-r[corner].y;
    for (int i=0;i<=16;++i) {
      double angle=pi+(corner+i/16.0)*pi/2;
      points.push_back({cx0+r[corner].x*std::cos(angle),cy0+r[corner].y*std::sin(angle)});
    }
  }
  if (reverse) std::reverse(points.begin(),points.end());
  std::vector<double> commands;
  for (size_t i=0;i<points.size();++i) {
    commands.push_back(i?1:0);commands.push_back(points[i].x);commands.push_back(points[i].y);
  }
  commands.push_back(2);
  return AppendCanvasPath(cx,self,commands.data(),uint32_t(commands.size()));
}
}
