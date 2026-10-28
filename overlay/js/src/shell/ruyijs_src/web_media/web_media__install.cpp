#include "web_media/audio_data.h"
#include "web_media/realm.h"
#include "web_media/state.h"
#include "web_media/media_codecs.h"
#include "web_media/stream.h"
#include "web_media/media_constraints.h"
#include "web_media/media_runtime.h"
#include "web_media/video_color_space.h"
#include "web_media/image_decoder.h"
#include "web_media/web_codecs.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
namespace ruyijs::web_media {
const JSClass kMediaClass={"MediaDevices",JSCLASS_HAS_RESERVED_SLOTS(1)};
namespace {
bool Illegal(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  NativeConstructorTrace trace(cx,args);
  return trace.Finish(WebKit::Fail(cx,"Illegal constructor."));
}
bool HandlerGet(JSContext* c,unsigned n,JS::Value* p) {return MediaHandler(c,n,p);}
}
Realm::Realm(JSContext* c,JS::HandleObject p,PageHelpers& h,interaction::Realm& e,
             browser_environment::Environment& b):state_(std::make_unique<State>(c,p,h,e,b)){}
Realm::~Realm()=default;
bool Realm::InstallWorker(bool codecs) {
  auto& s=*state_;JSAutoRealm realm(s.cx,s.kit.page);
  // A ServiceWorkerGlobalScope exposes MediaCapabilities only (Firefox 158).
  return InstallMediaBase(s) && InstallMediaCapabilities(s) &&
         (!codecs || (InstallVideoColorSpace(s) && InstallImageDecoder(s) && InstallAudioData(s) &&
                      InstallWebCodecs(s, true)));
}
bool Realm::Install() {
  auto& s=*state_;JSAutoRealm realm(s.cx,s.kit.page);
  JS::RootedObject parent(s.cx,s.events.prototype());
  if (!s.kit.Define("MediaDevices",&kMediaClass,parent,Illegal,&s.face) ||
      !s.kit.Method(s.face,"MediaDevices","getSupportedConstraints",MediaSupportedConstraints,0,0) ||
      !s.kit.Method(s.face,"MediaDevices","enumerateDevices",MediaMethod,0,0) ||
      !s.kit.Method(s.face,"MediaDevices","getUserMedia",MediaMethod,0,1) ||
      !s.kit.Method(s.face,"MediaDevices","getDisplayMedia",MediaDeviceRequest,0,0) ||
      !s.kit.Method(s.face,"MediaDevices","selectAudioOutput",MediaDeviceRequest,0,1) ||
      !s.kit.Accessor(s.face,"MediaDevices","ondevicechange",HandlerGet,MediaHandler,0) ||
      !MediaConstructorLast(s,s.face))
    return false;
  JS::RootedValue navigator(s.cx);
  if (!JS_GetProperty(s.cx,s.kit.page,"navigator",&navigator) || !navigator.isObject()) return false;
  s.navigator=&navigator.toObject();
  s.device=s.kit.New(&kMediaClass,s.face.prototype);
  if (!s.device || !s.events.Attach(s.device)) return false;
  JS::SetReservedSlot(s.device,0,JS::NullValue());
  JS::RootedObject proto(s.cx,s.environment.navigatorPrototype());
  JS::RootedObject getter(s.cx,s.kit.Function(NavigatorMedia,"get mediaDevices",0,0)),setter(s.cx);
  return getter && JS_DefineProperty(s.cx,proto,"mediaDevices",getter,setter,JSPROP_ENUMERATE) &&
         s.kit.helpers->Accessor("Navigator",proto,"mediaDevices") && InstallNavigatorMedia(s) &&
         InstallStreams(s) && InstallCanvasCapture(s) &&
         InstallMediaCodecs(s) &&
         InstallVideoColorSpace(s) &&
         InstallImageDecoder(s) && InstallAudioData(s) &&
         InstallWebCodecs(s, false) &&
         s.secure.Add(proto,"mozGetUserMedia") && s.secure.Add(proto,"mediaDevices") &&
         s.secure.Add(s.kit.page,"MediaDevices") && s.secure.Add(s.kit.page,"MediaDeviceInfo");
}
}
