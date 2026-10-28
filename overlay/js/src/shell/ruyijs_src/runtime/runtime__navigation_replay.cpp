#include "runtime/navigation_replay.h"
#include "runtime/resource_schema.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"
#include <cmath>
namespace ruyijs {
namespace {
using domain::execution_scheduling::NavigationReplayDocuments;
using domain::execution_scheduling::NavigationReplayFields;
bool Numeric(JSContext* cx, JS::HandleObject object, const char* name, std::optional<double>* target,
             bool bytes=false) {
  bool present=false;
  if(!JS_HasOwnProperty(cx,object,name,&present))return false;
  if(!present)return true;
  JS::RootedValue value(cx);
  if(!JS_GetProperty(cx,object,name,&value))return false;
  if(!value.isNumber()||!std::isfinite(value.toNumber())||value.toNumber()<0||
     value.toNumber()>(bytes?9007199254740991.0:1e12)||
     (bytes&&std::trunc(value.toNumber())!=value.toNumber()))
    return resource_wire::Error(cx,"Navigation replay field must be a bounded nonnegative number",true);
  *target=value.toNumber();
  return true;
}
bool Bytes(JSContext* cx, JS::HandleObject object, const char* name, std::optional<uint64_t>* target) {
  std::optional<double> value;
  if(!Numeric(cx,object,name,&value,true))return false;
  if(value)*target=uint64_t(*value);
  return true;
}
bool Ordered(std::optional<double> first,std::optional<double> second) {
  return !first||!second||*first<=*second;
}
bool ReadFields(JSContext* cx,JS::HandleObject input,NavigationReplayFields* fields) {
  if(!resource_wire::Fields(cx,input,{"requestStart","responseStart","responseEnd","loadEventEnd",
                                    "transferSize","encodedBodySize","decodedBodySize"})||
     !Numeric(cx,input,"requestStart",&fields->request_start)||
     !Numeric(cx,input,"responseStart",&fields->response_start)||
     !Numeric(cx,input,"responseEnd",&fields->response_end)||
     !Numeric(cx,input,"loadEventEnd",&fields->load_event_end)||
     !Bytes(cx,input,"transferSize",&fields->transfer_size)||
     !Bytes(cx,input,"encodedBodySize",&fields->encoded_size)||
     !Bytes(cx,input,"decodedBodySize",&fields->decoded_size))return false;
  if(!fields->Count())return resource_wire::Error(cx,"Navigation replay requires at least one measured field");
  const std::optional<double> values[]={fields->request_start,fields->response_start,
                                      fields->response_end,fields->load_event_end};
  for(unsigned i=0;i<4;++i)for(unsigned j=i+1;j<4;++j)
    if(!Ordered(values[i],values[j]))return resource_wire::Error(cx,"Recorded navigation times are inconsistent");
  return true;
}
bool Status(JSContext* cx,execution_scheduling::Realm& owner,JS::MutableHandleValue result) {
  const auto& entries=owner.NavigationReplayStatus();
  JS::RootedObject rows(cx,JS::NewArrayObject(cx,entries.size()));
  if(!rows)return false;
  for(uint32_t index=0;index<entries.size();++index) {
    const auto& entry=entries[index];
    JS::RootedObject row(cx,JS_NewPlainObject(cx));
    JS::RootedString url(cx,JS_NewStringCopyUTF8N(cx,JS::UTF8Chars(entry.url.data(),entry.url.size())));
    JS::RootedValue bound(cx,JS::BooleanValue(entry.bound));
    if(!row||!url||!JS_DefineProperty(cx,row,"url",url,JSPROP_ENUMERATE)||
       !JS_DefineProperty(cx,row,"bound",bound,JSPROP_ENUMERATE)||
       !JS_DefineProperty(cx,row,"fields",entry.fields.Count(),JSPROP_ENUMERATE)||
       !JS_SetElement(cx,rows,index,row))return false;
  }
  result.setObject(*rows);
  return true;
}
}
bool NavigationReplayRequest(JSContext* cx,execution_scheduling::Realm& owner,JS::HandleObject request,
                             bool configure,bool frozen,JS::MutableHandleValue result) {
  if(!configure)return resource_wire::Fields(cx,request,{"id","op","action"})&&Status(cx,owner,result);
  if(frozen)return resource_wire::Error(cx,"Navigation replay configuration is closed after execution");
  if(!resource_wire::Fields(cx,request,{"id","op","action","documents"}))return false;
  JS::RootedValue value(cx);
  bool array=false;
  if(!JS_GetProperty(cx,request,"documents",&value)||!JS::IsArrayObject(cx,value,&array))return false;
  if(!array)return resource_wire::Error(cx,"Navigation replay documents must be an array");
  JS::RootedObject list(cx,&value.toObject());
  uint32_t length=0;
  if(!JS::GetArrayLength(cx,list,&length))return false;
  if(length>32)return resource_wire::Error(cx,"Navigation replay exceeds 32 documents",true);
  NavigationReplayDocuments candidate;
  for(uint32_t index=0;index<length;++index) {
    JS::RootedObject object(cx),fields(cx);
    domain::execution_scheduling::NavigationReplayDocument entry;
    if(!JS_GetElement(cx,list,index,&value)||!resource_wire::Object(cx,value,&object)||
       !resource_wire::Fields(cx,object,{"url","fields"})||
       !resource_wire::Property(cx,object,"url",&entry.url,8192)||
       !JS_GetProperty(cx,object,"fields",&value)||!resource_wire::Object(cx,value,&fields))return false;
    const auto& url=entry.url;
    const size_t prefix=url.rfind("https://",0)==0?8:url.rfind("http://",0)==0?7:0;
    if(!prefix||url.size()<=prefix||url[prefix]=='/'||url.find('#')!=std::string::npos)
      return resource_wire::Error(cx,"Navigation replay requires an exact HTTP(S) document URL without fragment");
    for(unsigned char character:url)if(character<=32||character==127)
      return resource_wire::Error(cx,"Navigation replay URL contains a control or space");
    if(!ReadFields(cx,fields,&entry.fields))return false;
    candidate.push_back(std::move(entry));
  }
  if(!owner.ConfigureNavigationReplay(std::move(candidate)))return false;
  result.setBoolean(true);
  return true;
}
}
