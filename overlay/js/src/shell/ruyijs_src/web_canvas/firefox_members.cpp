// Firefox-specific Canvas members; state stays with the existing binding/context.
#include "web_canvas/internal.h"
#include "js/CallAndConstruct.h"

namespace ruyijs::web_canvas {
bool ReadContextOptions(JSContext* cx, JS::HandleObject context, JS::HandleValue value) {
  if (value.isNullOrUndefined()) return true;
  if (!value.isObject()) return WebKit::Fail(cx, "Canvas settings must be a dictionary");
  JS::RootedObject options(cx, &value.toObject());
  JS::RootedValue field(cx);
  if (!JS_GetProperty(cx, options, "alpha", &field)) return false;
  bool alpha = field.isUndefined() || JS::ToBoolean(field);
  // Gecko parses the dictionary, but does not implement these two options.
  if (!JS_GetProperty(cx, options, "colorSpace", &field)) return false;
  if (!field.isUndefined()) {
    JS::RootedString text(cx, JS::ToString(cx, field));
    bool srgb, p3;
    if (!text || !detail::StringEqualsAscii(cx,text,"srgb",&srgb) ||
        !detail::StringEqualsAscii(cx,text,"display-p3",&p3)) return false;
    if (!srgb && !p3) return WebKit::Fail(cx,"Invalid PredefinedColorSpace");
  }
  if (!JS_GetProperty(cx, options, "desynchronized", &field) ||
      !JS_GetProperty(cx, options, "willReadFrequently", &field)) return false;
  JS::SetReservedSlot(context, kContextAlphaSlot, JS::BooleanValue(alpha));
  JS::SetReservedSlot(context, kContextReadOftenSlot, JS::BooleanValue(JS::ToBoolean(field)));
  return true;
}
bool ContextSettings(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject self(cx), result(cx, JS_NewPlainObject(cx));
  if (!detail::ContextReceiver(cx,args,&self) || !result) return false;
  JS::RootedValue alpha(cx,JS::GetReservedSlot(self,kContextAlphaSlot));
  JS::RootedValue often(cx,JS::GetReservedSlot(self,kContextReadOftenSlot));
  JS::RootedString color(cx,JS_NewStringCopyZ(cx,"srgb"));
  if (!color || !JS_DefineProperty(cx,result,"alpha",alpha,JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx,result,"willReadFrequently",often,JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx,result,"desynchronized",false,JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx,result,"colorSpace",color,JSPROP_ENUMERATE)) return false;
  args.rval().setObject(*result);return true;
}
bool DrawFocus(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  State& s=*detail::Self(args);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx,args,&self)) return false;
  if (!args.get(0).isObject()) return WebKit::Fail(cx,"drawFocusIfNeeded requires Element");
  JS::RootedObject element(cx,&args[0].toObject());
  bool valid=false;
  if (s.elementCheck && !s.elementCheck(s.elementCheckContext,element,&valid)) return false;
  if (!valid) return WebKit::Fail(cx,"drawFocusIfNeeded requires Element");
  args.rval().setUndefined();return true;
}
namespace {
bool MozGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  State& s=*detail::Self(args);
  int op=WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!detail::CanvasReceiver(s,args.thisv(),&self, &args)) return false;
  if (op) {
    CanvasBinding* b=s.Find(self);
    args.rval().set(b?b->printCallback.get():JS::NullValue());return true;
  }
  JS::RootedValue value(cx);
  if (!detail::GetAttribute(s,self,"moz-opaque",&value)) return false;
  args.rval().setBoolean(!value.isNullOrUndefined());return true;
}
bool MozSet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  State& s=*detail::Self(args);
  int op=WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!detail::CanvasReceiver(s,args.thisv(),&self, &args)) return false;
  if (op) {
    JS::RootedValue value(cx,args.get(0));
    if (!value.isNullOrUndefined() && (!value.isObject() || !JS::IsCallable(&value.toObject())))
      return WebKit::Fail(cx,"mozPrintCallback requires a callable or null");
    s.FindOrCreate(self)->printCallback=value.isNullOrUndefined()?JS::NullValue():value.get();
  } else {
    JS::RootedString name(cx,JS_NewStringCopyZ(cx,"moz-opaque"));
    if (!name) return false;
    if (JS::ToBoolean(args.get(0))) {
      JS::RootedString empty(cx,JS_NewStringCopyZ(cx,""));
      if (!empty || !detail::SetAttribute(s,self,"moz-opaque",empty)) return false;
    } else {
      JS::RootedValueArray<1> values(cx);values[0].setString(name);
      JS::RootedValue ignored(cx);
      if (!detail::CallMethod(cx,self,"removeAttribute",values,&ignored)) return false;
    }
  }
  args.rval().setUndefined();return true;
}
}
bool InstallCanvasFirefox(State& s) {
  return detail::AccessorOn(s.kit,s.canvasPrototype,"HTMLCanvasElement","mozOpaque",MozGet,MozSet,0) &&
         detail::AccessorOn(s.kit,s.canvasPrototype,"HTMLCanvasElement","mozPrintCallback",MozGet,MozSet,1);
}
}
