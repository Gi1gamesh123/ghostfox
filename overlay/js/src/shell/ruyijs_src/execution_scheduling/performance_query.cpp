#include "execution_scheduling/performance.h"
#include "js/Array.h"
#include "js/MapAndSet.h"
namespace ruyijs::execution_scheduling {
bool PerfQuery(JSContext* cx,JS::HandleObject owner,const JS::CallArgs& args,PerfOp op) {
 bool clear=op==ClearMarks||op==ClearMeasures||op==ClearResources;
 if((op==ByName||op==ByType)&&!args.length())return Error(cx,"Performance query requires an argument");
 int kind=op==ClearMarks?0:op==ClearMeasures?1:op==ClearResources?2:-1;
 bool named=op==ByName||(clear&&op!=ClearResources&&!args.get(0).isUndefined());
 std::u16string text;JS::RootedString name(cx),type(cx);
 if(named&&(!PerfString(cx,args[0],&name)||!PerfText(cx,name,&text)))return false;
 if(op==ByType|| (op==ByName&&!args.get(1).isUndefined())){
  if(!PerfString(cx,args[op==ByType?0:1],&type))return false;
  bool mark=false,measure=false,resource=false,navigation=false;
  if(!JS_StringEqualsAscii(cx,type,"mark",&mark)||!JS_StringEqualsAscii(cx,type,"measure",&measure)||
     !JS_StringEqualsAscii(cx,type,"resource",&resource)||
     !JS_StringEqualsAscii(cx,type,"navigation",&navigation))return false;
  kind=mark?0:measure?1:resource?2:navigation?3:4;
 }
 std::vector<uint32_t> ids;
 auto* timeline=static_cast<Timeline*>(JS::GetReservedSlot(owner,PTimeline).toPrivate());
 if(!SelectTimeline(timeline,named?&text:nullptr,kind,clear,&ids)){JS_ReportOutOfMemory(cx);return false;}
 JS::RootedObject array(cx,clear?nullptr:JS::NewArrayObject(cx,0));
 if(!clear&&!array)return false;
 for(uint32_t id:ids){
  JS::RootedValue value(cx),key(cx,JS::NumberValue(id));
  {JSAutoRealm realm(cx,owner);JS::RootedObject map(cx,&JS::GetReservedSlot(owner,PEntries).toObject());
   if(clear){bool removed=false;if(!JS::MapDelete(cx,map,key,&removed))return false;}
   else if(!JS::MapGet(cx,map,key,&value))return false;
  }
  if(!clear){uint32_t length=0;
   if(!JS_WrapValue(cx,&value)||!JS::GetArrayLength(cx,array,&length)||!JS_SetElement(cx,array,length,value))return false;
  }
 }
 if(clear)args.rval().setUndefined();else args.rval().setObject(*array);
 return true;
}
}
