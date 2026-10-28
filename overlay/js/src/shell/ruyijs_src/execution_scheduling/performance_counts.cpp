#include "execution_scheduling/performance_observer.h"
#include "runtime/page_helpers.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/MapAndSet.h"
#include "js/Symbol.h"
#include "js/Wrapper.h"
namespace ruyijs::execution_scheduling {
const JSClass kEventCountsClass={"EventCounts",JSCLASS_HAS_RESERVED_SLOTS(1)};
namespace {
const char* const kKeys[]={"contextmenu","dblclick","mousedown","mouseenter","mouseleave","mouseout",
 "mouseover","mouseup","auxclick","click","pointerover","pointerenter","pointerdown","pointerup",
 "pointercancel","pointerout","pointerleave","gotpointercapture","lostpointercapture","touchstart",
 "touchend","touchcancel","keydown","keypress","keyup","beforeinput","input","compositionstart",
 "compositionupdate","compositionend","dragstart","dragend","dragenter","dragleave","dragover","drop"};
bool CountsSnapshot(JSContext* cx,JS::HandleObject map,JS::MutableHandle<JS::GCVector<JS::Value>> values){
 JSAutoRealm realm(cx,map);return js::GetMapObjectKeysAndValuesInterleaved(cx,map,values);
}
}
bool PerfCountsMethod(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);int op=PerfSelector(args);
 JS::RootedObject self(cx,args.thisv().isObject()?js::CheckedUnwrapStatic(&args.thisv().toObject()):nullptr);
 if(!self||JS::GetClass(self)!=&kEventCountsClass)return Error(cx,"Illegal EventCounts receiver");
 JS::RootedObject map(cx,&JS::GetReservedSlot(self,0).toObject());
 if(op==6){JSAutoRealm realm(cx,map);args.rval().setNumber(JS::MapSize(cx,map));return true;}
 if(op==4||op==5){
  JS::RootedString text(cx);if(!PerfString(cx,args.get(0),&text))return false;
  JS::RootedValue key(cx,JS::StringValue(text));
  {JSAutoRealm realm(cx,map);if(!JS_WrapValue(cx,&key))return false;
   if(op==4){bool found=false;if(!JS::MapHas(cx,map,key,&found))return false;args.rval().setBoolean(found);}
   else if(!JS::MapGet(cx,map,key,args.rval()))return false;}
  return JS_WrapValue(cx,args.rval());
 }
 if(op==3&&(!argc||!args[0].isObject()||!JS::IsCallable(&args[0].toObject())))
  return Error(cx,"EventCounts forEach requires a callback");
 JS::Rooted<JS::GCVector<JS::Value>> values(cx,JS::GCVector<JS::Value>(cx));if(!CountsSnapshot(cx,map,&values))return false;
 for(unsigned i=0;i<values.length();++i)if(!JS_WrapValue(cx,values[i]))return false;
 if(op==3){
  JS::RootedValue fn(cx,args[0]),receiver(cx,args.get(1)),ignored(cx);
  JS::RootedValueArray<3> arguments(cx);arguments[2].set(args.thisv());
  for(unsigned i=0;i<values.length();i+=2){arguments[0].set(values[i+1]);arguments[1].set(values[i]);
   if(!JS::Call(cx,receiver,fn,arguments,&ignored))return false;}
  args.rval().setUndefined();return true;
 }
 // The uncollected input baseline is immutable. A method-realm backing map
 // gives native iterator/result/pair factories the Firefox realm identity.
 JS::RootedObject local(cx,JS::NewMapObject(cx));if(!local)return false;
 for(unsigned i=0;i<values.length();i+=2)if(!JS::MapSet(cx,local,values[i],values[i+1]))return false;
 return op==0?JS::MapEntries(cx,local,args.rval()):op==1?JS::MapKeys(cx,local,args.rval()):JS::MapValues(cx,local,args.rval());
}
bool PerfInstallCounts(JSContext* cx,JS::HandleObject page,JS::HandleObject owner,PageHelpers& helpers){
 JS::RootedObject prototype(cx,JS_NewPlainObject(cx)),map(cx,JS::NewMapObject(cx));
 JS::RootedObject ctor(cx,PerfFunction(cx,owner,PerfIllegalConstructor,"EventCounts",0,0,JSFUN_CONSTRUCTOR));
 if(!prototype||!map||!ctor)return false;
 const char* const names[]={"entries","keys","values","forEach","has","get"};
 for(int i=0;i<6;++i){JS::RootedObject fn(cx,PerfFunction(cx,owner,PerfCountsMethod,names[i],i>=3?1:0,i));
  if(!fn||!JS_DefineProperty(cx,prototype,names[i],fn,JSPROP_ENUMERATE)||
     !helpers.Member(fn,"EventCounts",names[i],"call"))return false;}
 if(!PerfProperty(cx,helpers,owner,prototype,"EventCounts","size",PerfCountsMethod,6)||
    !PerfFinishInterface(cx,helpers,"EventCounts",page,prototype,ctor))return false;
 JS::RootedValue entries(cx);JS::RootedId iterator(cx,JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx,JS::SymbolCode::iterator)));
 if(!JS_GetProperty(cx,prototype,"entries",&entries)||!JS_DefinePropertyById(cx,prototype,iterator,entries,0))return false;
 JS::RootedValue zero(cx,JS::Int32Value(0));
 for(const char* name:kKeys){JS::RootedString text(cx,JS_NewStringCopyZ(cx,name));if(!text)return false;
  JS::RootedValue key(cx,JS::StringValue(text));if(!JS::MapSet(cx,map,key,zero))return false;}
 JS::RootedObject object(cx,JS_NewObjectWithGivenProto(cx,&kEventCountsClass,prototype));if(!object)return false;
 JS::SetReservedSlot(object,0,JS::ObjectValue(*map));JS::SetReservedSlot(owner,PCounts,JS::ObjectValue(*object));
 return true;
}
}
