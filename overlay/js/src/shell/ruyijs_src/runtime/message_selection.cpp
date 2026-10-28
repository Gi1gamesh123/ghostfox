#include "runtime/scheduling_control.h"
#include "runtime/environment_control.h"
#include "runtime/resource_schema.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include <cmath>
#include <initializer_list>
namespace ruyijs {
namespace {
bool Fields(JSContext* cx, JS::HandleObject o, std::initializer_list<const char*> allowed) {
  JS::Rooted<JS::IdVector> keys(cx, JS::IdVector(cx)); if (!JS_Enumerate(cx, o, &keys)) return false;
  for (size_t i=0;i<keys.length();++i) { JS::RootedId id(cx,keys[i]); JS::RootedValue v(cx); if(!JS_IdToValue(cx,id,&v))return false;
    JS::RootedString k(cx,JS::ToString(cx,v));std::string n;if(!k||!ToUTF8(cx,k,&n))return false;if(n=="id"||n=="op"||n=="action")continue;
    bool ok=false;for(const char* a:allowed)if(n==a)ok=true;if(!ok)return execution_scheduling::Error(cx,"Unknown message selection field");}
  return true;
}
bool Num(JSContext* cx,JS::HandleObject o,const char* name,double* out){JS::RootedValue v(cx);if(!JS_GetProperty(cx,o,name,&v))return false;if(!v.isNumber()||!std::isfinite(v.toNumber()))return execution_scheduling::Error(cx,"Message selection needs finite numbers");*out=v.toNumber();return true;}
struct List { JSContext* cx; JS::RootedObject array; uint32_t count=0; explicit List(JSContext* c):cx(c),array(c,JS::NewArrayObject(c,0)){} };
bool Append(void* p,const execution_scheduling::MessageSnapshot& v){auto&l=*static_cast<List*>(p);JSContext*cx=l.cx;JS::RootedObject r(cx,JS_NewPlainObject(cx));if(!r)return false; if(!JS_DefineProperty(cx,r,"task_id",double(v.taskId),JSPROP_ENUMERATE)||!JS_DefineProperty(cx,r,"sequence",double(v.sequence),JSPROP_ENUMERATE)||!JS_DefineProperty(cx,r,"source_realm",v.sourceRealm,JSPROP_ENUMERATE)||!JS_DefineProperty(cx,r,"target_realm",v.targetRealm,JSPROP_ENUMERATE)||!JS_DefineProperty(cx,r,"kind",v.kind,JSPROP_ENUMERATE)||!JS_DefineProperty(cx,r,"due_ms",v.deadline,JSPROP_ENUMERATE)||!JS_DefineProperty(cx,r,"ready",v.ready,JSPROP_ENUMERATE))return false;return JS_SetElement(cx,l.array,l.count++,r);}
}
bool SchedulingControl::MessageSelection(JS::HandleObject request,const std::string&action,JS::MutableHandleValue result){
 if(action=="configure_message_replay"){if(!Fields(cx_,request,{"enabled"}))return false;JS::RootedValue v(cx_);if(!JS_GetProperty(cx_,request,"enabled",&v)||!v.isBoolean())return execution_scheduling::Error(cx_,"Message replay enabled must be boolean");if(environment_.Frozen()||!owner_.ConfigureMessageReplay(v.toBoolean()))return false;result.set(v);return true;}
 if(action=="pending_messages"){if(!Fields(cx_,request,{}))return false;List l(cx_);if(!owner_.VisitPendingMessages(Append,&l))return false;result.setObject(*l.array);return true;}
 if(!Fields(cx_,request,{"task_id","source_realm","target_realm","sequence","kind","at_ms"}))return false;double id,s,t,q,k,at;if(!Num(cx_,request,"task_id",&id)||!Num(cx_,request,"source_realm",&s)||!Num(cx_,request,"target_realm",&t)||!Num(cx_,request,"sequence",&q)||!Num(cx_,request,"kind",&k)||!Num(cx_,request,"at_ms",&at))return false;if(id<1||s<1||t<1||q<1||k<0||std::trunc(id)!=id||std::trunc(s)!=s||std::trunc(t)!=t||std::trunc(q)!=q||std::trunc(k)!=k)return execution_scheduling::Error(cx_,"Message identity must be bounded integers",true);environment_.Freeze();execution_scheduling::RunResult rr;if(!owner_.RunSelectedMessage(uint64_t(id),uint32_t(s),uint32_t(t),uint64_t(q),uint32_t(k),at,&rr))return false;return Describe(rr,result);
}
}
