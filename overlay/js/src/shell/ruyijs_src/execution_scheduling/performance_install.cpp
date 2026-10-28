#include "execution_scheduling/performance.h"
#include "runtime/page_helpers.h"
#include "jsfriendapi.h"
#include "js/Array.h"
namespace ruyijs::execution_scheduling {
bool PerfInstallEntries(State& s,JS::HandleObject page,JS::HandleObject owner,PageHelpers& helpers) {
 JSContext* cx=s.cx;
 JS::RootedObject prototypes(cx,JS::NewArrayObject(cx,KindCount));
 JS::RootedObjectVector constructors(cx);
 if(!prototypes||!constructors.resize(KindCount))return false;
 JS::SetReservedSlot(owner,PPrototypes,JS::ObjectValue(*prototypes));
 for(int kind=0;kind<KindCount;++kind){
  const auto& schema=kPerfSchema[kind];
  if(s.exposure==Exposure::Worker&&!schema.worker)continue;
  JS::RootedObject proto(cx,JS_NewPlainObject(cx)),ctor(cx),parent(cx);
  JS::RootedValue value(cx);
  if(!proto)return false;
  if(schema.parent>=0){
   if(!JS_GetElement(cx,prototypes,schema.parent,&value))return false;
   parent=&value.toObject();if(!JS_SetPrototype(cx,proto,parent))return false;
  }
  ctor=PerfFunction(cx,owner,kind==Mark?PerfMarkConstructor:PerfIllegalConstructor,
      schema.name,kind==Mark?1:0,kind,JSFUN_CONSTRUCTOR);
  if(!ctor||(schema.parent>=0&&!JS_SetPrototype(cx,ctor,constructors[schema.parent])))return false;
  constructors[kind].set(ctor);
  if(schema.json){
   JS::RootedObject fn(cx,PerfFunction(cx,owner,kind>=LegacyTiming?PerfLegacyMember:PerfEntryJson,
       "toJSON",0,kind>=LegacyTiming?kind*100+99:kind));
   if(!fn||!JS_DefineProperty(cx,proto,"toJSON",fn,JSPROP_ENUMERATE)||
      !helpers.Member(fn,schema.name,"toJSON","call"))return false;
  }
  for(unsigned field=0;field<schema.count;++field)
   if(!PerfProperty(cx,helpers,owner,proto,schema.name,schema.fields[field],
       kind>=LegacyTiming?PerfLegacyMember:PerfEntryMember,kind*100+field))return false;
  if(kind==LegacyNavigation){
   const char* const names[]={"TYPE_NAVIGATE","TYPE_RELOAD","TYPE_BACK_FORWARD","TYPE_RESERVED"};
   for(unsigned i=0;i<4;++i)
    if(!JS_DefineProperty(cx,proto,names[i],i==3?255:i,JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT)||
       !JS_DefineProperty(cx,ctor,names[i],i==3?255:i,JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT))return false;
  }
  if(!PerfFinishInterface(cx,helpers,schema.name,page,proto,ctor)||!JS_SetElement(cx,prototypes,kind,proto))return false;
  if(kind>=LegacyTiming){
   JS::RootedObject object(cx,JS_NewObjectWithGivenProto(cx,kind==LegacyTiming?&kLegacyTimingClass:&kLegacyNavigationClass,proto));
   if(!object)return false;
   JS::SetReservedSlot(object,0,JS::ObjectValue(*owner));
   JS::SetReservedSlot(owner,kind==LegacyTiming?PTiming:PNavigation,JS::ObjectValue(*object));
  }
 }
 return true;
}
}
