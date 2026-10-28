#include "execution_scheduling/idle.h"
#include "runtime/page_helpers.h"
#include "jsfriendapi.h"
#include "js/Symbol.h"
namespace ruyijs::execution_scheduling {
const JSClass kIdleFactoryClass={"IdleFactory",JSCLASS_HAS_RESERVED_SLOTS(IFactorySlots)};
JSObject* IdleFunction(JSContext* cx,JS::HandleObject factory,JSNative native,const char* name,unsigned length,int op,unsigned flags){
 JS::RootedFunction fn(cx,js::NewFunctionWithReserved(cx,native,length,flags,name));if(!fn)return nullptr;
 JS::RootedObject object(cx,JS_GetFunctionObject(fn));
 js::SetFunctionNativeReserved(object,0,JS::ObjectValue(*factory));
 js::SetFunctionNativeReserved(object,1,JS::Int32Value(op));return object;
}
bool InstallIdle(State& s,JS::HandleObject page,PageHelpers& helpers){
 JSContext* cx=s.cx;
 JS::RootedObject factory(cx,JS_NewObjectWithGivenProto(cx,&kIdleFactoryClass,nullptr));
 JS::RootedObject prototype(cx,JS_NewPlainObject(cx));if(!factory||!prototype)return false;
 JS::SetReservedSlot(factory,IState,JS::PrivateValue(&s));JS::SetReservedSlot(factory,IPage,JS::ObjectValue(*page));
 JS::SetReservedSlot(factory,IPrototype,JS::ObjectValue(*prototype));JS::SetReservedSlot(factory,INext,JS::DoubleValue(1));
 JS::RootedObject ctor(cx,IdleFunction(cx,factory,IdleIllegalConstructor,"IdleDeadline",0,0,JSFUN_CONSTRUCTOR));
 JS::RootedObject time(cx,IdleFunction(cx,factory,IdleMember,"timeRemaining",0,0));
 JS::RootedObject timeout(cx,IdleFunction(cx,factory,IdleMember,"get didTimeout",0,1));
 JS::RootedObject request(cx,IdleFunction(cx,factory,IdleRequest,"requestIdleCallback",1,0));
 JS::RootedObject cancel(cx,IdleFunction(cx,factory,IdleCancel,"cancelIdleCallback",1,0));
 JS::RootedObject noSetter(cx);JS::RootedString tag(cx,JS_NewStringCopyZ(cx,"IdleDeadline"));
 JS::RootedId id(cx,JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx,JS::SymbolCode::toStringTag)));
 if(!ctor||!time||!timeout||!request||!cancel||!tag||
    !JS_DefineProperty(cx,prototype,"timeRemaining",time,JSPROP_ENUMERATE)||
    !JS_DefineProperty(cx,prototype,"didTimeout",timeout,noSetter,JSPROP_ENUMERATE)||
    !JS_DefineProperty(cx,prototype,"constructor",ctor,0)||
    !JS_DefinePropertyById(cx,prototype,id,tag,JSPROP_READONLY)||
    !JS_DefineProperty(cx,ctor,"prototype",prototype,JSPROP_READONLY|JSPROP_PERMANENT)||
    !JS_DefineProperty(cx,page,"requestIdleCallback",request,JSPROP_ENUMERATE)||
    !JS_DefineProperty(cx,page,"cancelIdleCallback",cancel,JSPROP_ENUMERATE)||
    !JS_DefineProperty(cx,page,"IdleDeadline",ctor,0))return false;
 return helpers.Interface("IdleDeadline",prototype)&&helpers.Constructor("IdleDeadline",ctor)&&
  helpers.Member(time,"IdleDeadline","timeRemaining","call")&&helpers.Accessor("IdleDeadline",prototype,"didTimeout")&&
  helpers.Member(request,"Window","requestIdleCallback","call")&&helpers.Member(cancel,"Window","cancelIdleCallback","call")&&
  s.idleFactories.append(factory);
}
}
