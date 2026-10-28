#include "web_indexeddb/keys.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
#include <cmath>
namespace ruyijs::web_indexeddb {
namespace {
bool CountValue(JSContext* cx,JS::HandleValue value,uint32_t* out) {
  if(value.isUndefined())return true;
  double number;
  if(!JS::ToNumber(cx,value,&number))return false;
  if(!std::isfinite(number))return WebKit::Fail(cx,"IndexedDB count is outside unsigned long range");
  number=std::trunc(number);
  if(number<0 || number>4294967295.0)return WebKit::Fail(cx,"IndexedDB count is outside unsigned long range");
  *out=uint32_t(number);return true;
}
bool Options(JSContext* cx,JS::HandleValue value,Query* out,JS::MutableHandleValue query) {
  query.setUndefined();out->count=0;
  if(value.isNullOrUndefined())return true;
  if(!value.isObject())return WebKit::Fail(cx,"IndexedDB getAll options require a dictionary");
  JS::RootedObject options(cx,&value.toObject());JS::RootedValue count(cx),direction(cx);
  if(!JS_GetProperty(cx,options,"count",&count) || !CountValue(cx,count,&out->count) ||
     !JS_GetProperty(cx,options,"direction",&direction))return false;
  if(!direction.isUndefined()) {
    std::u16string text;
    if(!Text(cx,direction,&text))return false;
    if(text!=u"next" && text!=u"nextunique" && text!=u"prev" && text!=u"prevunique")
      return WebKit::Fail(cx,"Invalid IndexedDB cursor direction");
    out->reverse=text==u"prev" || text==u"prevunique";
  }
  return JS_GetProperty(cx,options,"query",query);
}
}
bool PrepareQuery(JSContext* cx,const JS::CallArgs& args,int kind,Query* out,JS::MutableHandleValue value) {
  value.set(args.get(0));
  if((kind==Get || kind==GetKey || kind==Put || kind==Add) && !args.length())
    return WebKit::Fail(cx,"IndexedDB argument is required");
  if(kind==GetAll || kind==GetAllKeys)return CountValue(cx,args.get(1),&out->count);
  if(kind==GetAllRecords) {
    JS::RootedValue input(cx,value);
    return Options(cx,input,out,value);
  }
  return true;
}
bool ReadQuery(State& s,JS::HandleValue value,int kind,Query* out) {
  JSContext* cx=s.cx;
  if(kind==Get || kind==GetKey || kind==Count || kind==GetAllRecords) {
    if(kind==Get || kind==GetKey)out->count=1;
    return ReadRange(s,value,&out->range,kind==Count || kind==GetAllRecords);
  }
  bool keyType=false;
  if(!KeyType(cx,value,&keyType))return false;
  JS::RootedObject raw(cx,value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr);
  if(value.isNullOrUndefined() || keyType || (raw && JS::GetClass(raw)==&kKeyRangeClass))
    return ReadRange(s,value,&out->range,true);
  JS::RootedValue query(cx);
  return Options(cx,value,out,&query) && ReadRange(s,query,&out->range,true);
}
}
