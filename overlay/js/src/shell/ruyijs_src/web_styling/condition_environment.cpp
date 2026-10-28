#include "web_styling/condition_environment.h"
#include "web_styling/internal.h"
#include "browser_environment/environment.h"
#include "js/Realm.h"
#include "web_styling/media_feature_profile.h"
namespace ruyijs::web_styling {
bool ReadConditionEnvironment(State& state,ruyijs::domain::styling::CssMediaEnvironment* out) {
  JSContext* cx=state.cx;JSAutoRealm realm(cx,state.page);
  double scroll_x,scroll_y;
  if(!state.environment->ReadLayoutViewport(&out->width,&out->height,&scroll_x,&scroll_y)) return false;
  JS::RootedValue snapshot(cx),group(cx),value(cx);
  if(!state.environment->Describe(&snapshot)||!snapshot.isObject()) return false;
  JS::RootedObject object(cx,&snapshot.toObject());
  if(!JS_GetProperty(cx,object,"screen",&group)||!group.isObject()) return false;
  JS::RootedObject screen(cx,&group.toObject());
  const char* fields[]={"width","height","colorDepth"};
  double* values[]={&out->deviceWidth,&out->deviceHeight,&out->color};
  for(size_t i=0;i<3;++i) {
    if(!JS_GetProperty(cx,screen,fields[i],&value)) return false;
    if(!value.isNumber()) return WebKit::Fail(cx,"Configured display value is not numeric");
    *values[i]=value.toNumber();
  }
  out->color/=3;
  // The same profile value window.devicePixelRatio reports: the environment
  // owner's viewport group is the one source of the device pixel ratio, so a
  // configured ratio moves `resolution` and `-webkit-device-pixel-ratio` too.
  if(!JS_GetProperty(cx,object,"viewport",&group)||!group.isObject()) return false;
  JS::RootedObject viewport(cx,&group.toObject());
  if(!JS_GetProperty(cx,viewport,"devicePixelRatio",&value)||!value.isNumber())
    return WebKit::Fail(cx,"Configured devicePixelRatio is not numeric");
  out->ratio=value.toNumber();
  out->touch=state.environment->TouchCapable();
  out->profile=MediaFeatureProfile();
  return true;
}
}
