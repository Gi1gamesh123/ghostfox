#include "resource_network/state.h"
#include "resource_network/abort.h"
#include "interaction/realm.h"
#include "runtime/page_helpers.h"
namespace ruyijs::resource_network {
namespace {bool Illegal(JSContext* cx,unsigned,JS::Value*){return Error(cx,"Illegal constructor");}}
bool InstallAbortSignal(State& s,PageHelpers& helpers,interaction::Realm& events,JS::MutableHandleObject result){
 JSContext* cx=s.cx;s.events=&events;JS::RootedObject base(cx,events.prototype());
 JS::RootedObject proto(cx,JS_NewPlainObject(cx));
 JS::RootedObject ctor(cx,NativeFunction(s,Illegal,"AbortSignal",0,JSFUN_CONSTRUCTOR));
 JS::RootedObject method(cx,NativeSlot(s,AbortMember,"throwIfAborted",0,2));
 if(!proto||!ctor||!method||!JS_SetPrototype(cx,proto,base)||!JS_GetProperty(cx,s.page,"DOMException",&s.abortException)||
  !JS_GetProperty(cx,base,"addEventListener",&s.abortAdd)||!JS_GetProperty(cx,base,"removeEventListener",&s.abortRemove)||
  !JS_DefineProperty(cx,proto,"throwIfAborted",method,JSPROP_ENUMERATE)||!helpers.Member(method,"AbortSignal","throwIfAborted","call"))return false;
 const char* names[]={"aborted","reason","onabort"};
 for(int i=0;i<3;++i){std::string label=std::string("get ")+names[i];
  JS::RootedObject get(cx,NativeSlot(s,AbortMember,label.c_str(),0,i==2?3:i)),set(cx);
  if(i==2)set=NativeSlot(s,AbortMember,"set onabort",1,4);
  if(!get||(i==2&&!set)||!JS_DefineProperty(cx,proto,names[i],get,set,JSPROP_ENUMERATE)||!helpers.Accessor("AbortSignal",proto,names[i]))return false;}
 const char* statics[]={"abort","timeout","any"};
 for(int i=0;i<3;++i){JS::RootedObject fn(cx,NativeSlot(s,AbortStaticMethod,statics[i],i?1:0,i));
  if(!fn||!JS_DefineProperty(cx,ctor,statics[i],fn,JSPROP_ENUMERATE)||!helpers.Member(fn,"AbortSignal",statics[i],"call"))return false;}
 JS::RootedValue baseCtor(cx);if(!JS_GetProperty(cx,base,"constructor",&baseCtor))return false;
 JS::RootedObject ctorBase(cx,&baseCtor.toObject());
 if(!JS_SetPrototype(cx,ctor,ctorBase)||!JS_DefineProperty(cx,ctor,"prototype",proto,JSPROP_READONLY|JSPROP_PERMANENT)||
    !JS_DefineProperty(cx,proto,"constructor",ctor,0)||!Tag(cx,proto,"AbortSignal")||!helpers.Constructor("AbortSignal",ctor))return false;
 s.signalPrototype=proto;result.set(ctor);return true;
}
}
