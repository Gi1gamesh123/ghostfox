#include "execution_scheduling/performance.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
namespace ruyijs::execution_scheduling {
bool PerfMethod(JSContext* cx,unsigned argc,JS::Value* vp) {
 auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject self(cx);
 if(!PerfReceiver(cx,args,&self))return false;
 int selector=PerfSelector(args);PerfOp op=PerfOp(selector>=100?selector-100:selector);
 auto* state=static_cast<State*>(JS::GetReservedSlot(self,PClock).toPrivate());
 const char* const names[]={"now","toJSON","getEntries","getEntriesByType","getEntriesByName",
  "clearResourceTimings","setResourceTimingBufferSize","mark","clearMarks","measure","clearMeasures",
  "timeOrigin","timing","navigation","onresourcetimingbufferfull","interactionCount","eventCounts"};
 state->Observe("Performance",names[op],int(selector>=100?ApiAccess::Write:op>=Origin?ApiAccess::Read:ApiAccess::Call));
 switch(op){
 case Now:args.rval().setDouble(PerfNow(self));return true;
 case InteractionCount:args.rval().setInt32(0);return true;
 case EventCountsObject:args.rval().set(JS::GetReservedSlot(self,PCounts));return JS_WrapValue(cx,args.rval());
 case Origin:args.rval().setDouble(PerfTimeOrigin(self));return true;
 case TimingObject:case NavigationObject:
  args.rval().set(JS::GetReservedSlot(self,op==TimingObject?PTiming:PNavigation));return JS_WrapValue(cx,args.rval());
 case Handler:
  if(selector<100){args.rval().set(JS::GetReservedSlot(self,PHandler));return JS_WrapValue(cx,args.rval());}
  if(!argc)return Error(cx,"Performance handler setter requires a value");
  args.rval().setUndefined();return PerfSetHandler(cx,self,args[0]);
 case AddMark:return PerfMark(cx,self,args,args.rval());
 case AddMeasure:return PerfMeasure(cx,self,args,args.rval());
 case Entries:case ByType:case ByName:case ClearMarks:case ClearMeasures:case ClearResources:return PerfQuery(cx,self,args,op);
 case BufferSize:{
  if(!argc)return Error(cx,"setResourceTimingBufferSize requires a size");
  uint32_t size;if(!JS::ToUint32(cx,args[0],&size))return false;
  SetResourceLimit(static_cast<Timeline*>(JS::GetReservedSlot(self,PTimeline).toPrivate()),size);
  args.rval().setUndefined();return true;}
 case Json:{
  JS::RootedObject json(cx,JS_NewPlainObject(cx));
  JS::RootedValue timing(cx,JS::GetReservedSlot(self,PTiming)),navigation(cx,JS::GetReservedSlot(self,PNavigation));
  state->SyncClock();
  if(!json||!JS_DefineProperty(cx,json,"timeOrigin",PerfTimeOrigin(self),JSPROP_ENUMERATE))return false;
  if(state->exposure==Exposure::Window&&(!JS_WrapValue(cx,&timing)||!JS_WrapValue(cx,&navigation)||
    !JS_DefineProperty(cx,json,"timing",timing,JSPROP_ENUMERATE)||
    !JS_DefineProperty(cx,json,"navigation",navigation,JSPROP_ENUMERATE)||
    !JS_DefineProperty(cx,json,"interactionCount",0,JSPROP_ENUMERATE)))return false;
  args.rval().setObject(*json);return true;}
 }
 return Error(cx,"Unknown Performance operation");
}
}
