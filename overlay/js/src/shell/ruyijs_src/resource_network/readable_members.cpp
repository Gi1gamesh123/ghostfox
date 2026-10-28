#include "runtime/web_kit.h"
#include "resource_network/readable_stream.h"
#include "resource_network/readable_byte.h"
#include "resource_network/state.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
bool StreamLocked(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject stream(cx,StreamReceiver(args.thisv()));
  if (!stream) return WebKit::FailBrand(cx,args,"ReadableStream");
  args.rval().setBoolean(!JS::GetReservedSlot(stream,SReader).isUndefined());
  return true;
}
bool StreamGetReader(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject stream(cx,StreamReceiver(args.thisv()));
  if (!stream) return WebKit::FailBrand(cx,args,"ReadableStream");
  bool byob=false;
  if (!args.get(0).isNullOrUndefined()) {
    if (!args[0].isObject()) return Error(cx,"Reader options must be a dictionary");
    JS::RootedObject options(cx,&args[0].toObject());
    JS::RootedValue mode(cx);
    if (!JS_GetProperty(cx,options,"mode",&mode)) return false;
    if (!mode.isUndefined()) {
      std::string text;
      if (!Convert(cx,mode,&text)) return false;
      if (text!="byob") return Error(cx,"ReadableStream reader mode must be byob");
      byob=true;
    }
  }
  JS::RootedObject reader(cx);
  {
    JSAutoRealm realm(cx,stream);
    JS::RootedObject proto(cx,SlotObject(stream,byob?SByobPrototype:SReaderPrototype));
    if(byob && JS::GetReservedSlot(stream,SReader).isUndefined() && !IsByteStream(stream))
      return Error(cx,"ReadableStream.getReader: Trying to read with incompatible controller");
    if (!NewReader(cx,stream,proto,byob,&reader)) return false;
  }
  args.rval().setObject(*reader);
  return JS_WrapValue(cx,args.rval());
}
bool StreamCancel(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject stream(cx,StreamReceiver(args.thisv()));
  if (!stream) return WebKit::FailBrand(cx,args,"ReadableStream");
  JS::RootedObject methodGlobal(cx,JS::CurrentGlobalOrNull(cx));
  {
    JSAutoRealm realm(cx,stream);
    if (!JS::GetReservedSlot(stream,SReader).isUndefined()) {
      if (!TypeErrorPromise(cx,"ReadableStream is locked",args.rval())) return false;
    } else if (CancelReadable(cx,stream,args.get(0),args.rval(),methodGlobal)!=CallbackResult::Success) return false;
  }
  return JS_WrapValue(cx,args.rval());
}
bool StreamPipeTo(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  if (!StreamReceiver(args.thisv())) return WebKit::FailBrand(cx,args,"ReadableStream");
  // Preserve the existing public entry; real piping is the separately tracked P1 slice.
  return Settled(cx,JS::UndefinedHandleValue,args.rval());
}
}
