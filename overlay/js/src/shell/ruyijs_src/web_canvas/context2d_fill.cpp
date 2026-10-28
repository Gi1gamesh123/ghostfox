// fill(), fillRect() and fillText() rasterize through the domain's analytic
// scan converter; hit testing projects the recorded path rather than installing
// a second owner.
#include <cmath>
#include "web_canvas/internal.h"
#include "web_canvas/path_geometry.h"

namespace ruyijs::web_canvas {
namespace {
bool FillRule(JSContext* cx, JS::HandleValue value, bool* evenodd) {
  *evenodd=false;
  if (value.isUndefined()) return true;
  JS::RootedString text(cx,JS::ToString(cx,value));
  bool nonzero=false;
  if (!text || !detail::StringEqualsAscii(cx,text,"evenodd",evenodd) ||
      !detail::StringEqualsAscii(cx,text,"nonzero",&nonzero)) return false;
  return *evenodd || nonzero || WebKit::Fail(cx,"Invalid CanvasFillRule value");
}
bool FillMethod(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  const int operation=detail::ContextSlot(args);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx,args,&self)) return false;
  State& state=*detail::Self(args);
  bool evenodd=false,usable=false;
  double values[4];
  JS::RootedObject pathOwner(cx, self);
  unsigned offset = 0;
  if ((operation == 0 || operation == 3) && args.get(0).isObject() &&
      JS::GetClass(&args[0].toObject()) == &kPath2DClass) {
    pathOwner = &args[0].toObject();
    offset = 1;
  }
  if (operation==0 || operation==3) {
    if (operation==3 && argc<offset+2) return WebKit::Fail(cx,"isPointInPath requires two arguments");
    if (operation==3 && !detail::ReadNumbers(cx,args,offset,2,values,&usable)) return false;
    if (!FillRule(cx,args.get(offset+(operation==3?2:0)),&evenodd)) return false;
    usable = true;
  } else if (operation==1) {
    if (argc<4) return WebKit::Fail(cx,"fillRect requires four arguments");
    if (!detail::ReadNumbers(cx,args,0,4,values,&usable)) return false;
  } else {
    if (argc<3) return WebKit::Fail(cx,"fillText requires three arguments");
    JS::RootedString text(cx,JS::ToString(cx,args.get(0)));
    if (!text || !detail::ReadNumbers(cx,args,1,argc>3?3:2,values,&usable)) return false;
    if (usable) {
      std::u16string units;
      if (!detail::StringUnits(cx, text, &units)) return false;
      const double maxWidth = argc > 3 ? values[2] : NAN;
      if (!FillCanvasText(state, self, units, values[0], values[1], maxWidth)) return false;
    }
  }
  args.rval().setUndefined();
  // The painting operations of this file rasterize through the existing paint owner.
  if (operation==0) {
    if (!usable) return true;
    return FillRecordedPath(state,self,pathOwner,evenodd);
  }
  if (operation==1) {
    if (!usable) return true;
    return FillRecordedRect(state,self,values);
  }
  if (operation!=3) return true;
  args.rval().setBoolean(false);
  if (!usable) return true;
  if (offset) {
    // An explicit Path2D is transformed at use time, unlike the default path.
    double m[6];
    if (!detail::GetMatrix(cx, self, m)) return false;
    const double det=m[0]*m[3]-m[1]*m[2];
    if (det==0 || !std::isfinite(det)) return true;
    const double x=values[0]-m[4], y=values[1]-m[5];
    values[0]=(m[3]*x-m[2]*y)/det;
    values[1]=(m[0]*y-m[1]*x)/det;
  }
  JS::RootedObject path(cx,&JS::GetReservedSlot(pathOwner,kContextPathSlot).toObject());
  uint32_t length=0;
  if (!JS::GetArrayLength(cx,path,&length)) return false;
  std::vector<double> commands; commands.reserve(length);
  for (uint32_t i=0;i<length;++i) {
    JS::RootedValue value(cx);
    if (!JS_GetElement(cx,path,i,&value)) return false;
    commands.push_back(value.toNumber());
  }
  args.rval().setBoolean(PathContains(commands,values[0],values[1],evenodd));
  return true;
}
}
bool InstallContext2DFill(State& state,WebFace& face) {
  const char* names[]={"fill","fillRect","fillText","isPointInPath"};
  const unsigned lengths[]={0,4,3,2};
  for (int i=0;i<4;++i)
    if (!detail::Method2D(state,face,names[i],FillMethod,lengths[i],i))
      return false;
  return true;
}
}
