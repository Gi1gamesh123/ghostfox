#include "execution_scheduling/performance.h"
#include "js/CallAndConstruct.h"
#include "js/CharacterEncoding.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/StructuredClone.h"
#include "js/String.h"
#include <cmath>
namespace ruyijs::execution_scheduling {
bool PerfString(JSContext* cx,JS::HandleValue value,JS::MutableHandleString out) {
 out.set(JS::ToString(cx,value));return !!out;
}
bool PerfText(JSContext* cx,JS::HandleString text,std::u16string* out) {
 size_t length=JS_GetStringLength(text);out->resize(length);
 return JS_CopyStringChars(cx,mozilla::Range<char16_t>(out->data(),length),text);
}
bool PerfNumber(JSContext* cx,JS::HandleValue value,double* out) {
 return JS::ToNumber(cx,value,out)&&(std::isfinite(*out)||Error(cx,"Performance time must be finite"));
}
bool PerfOption(JSContext* cx,JS::HandleValue value,const char* name,JS::MutableHandleValue out) {
 out.setUndefined();if(value.isNullOrUndefined())return true;
 if(!value.isObject())return Error(cx,"Performance options must be an object");
 JS::RootedObject object(cx,&value.toObject());return JS_GetProperty(cx,object,name,out);
}
bool PerfReserved(JSContext* cx,JS::HandleString name) {
 for(const char* field:kLegacyTimingFields){bool equal=false;
  if(!JS_StringEqualsAscii(cx,name,field,&equal))return true;
  if(equal)return true;
 }return false;
}
bool PerfThrow(JSContext* cx,JS::HandleObject owner,const char* name) {
 JS::RootedValue ctor(cx,JS::GetReservedSlot(owner,PException));
 JS::RootedString message(cx,JS_NewStringCopyZ(cx,name));
 JS::RootedValueArray<2> args(cx);JS::RootedObject error(cx);
 if(!message||!JS_WrapValue(cx,&ctor))return false;
 args[0].setString(message);args[1].setString(message);
 if(!JS::Construct(cx,ctor,args,&error))return false;
 JS::RootedValue value(cx,JS::ObjectValue(*error));JS_SetPendingException(cx,value);return false;
}
bool PerfClone(JSContext* cx,JS::HandleObject owner,JS::HandleValue value,JS::MutableHandleValue result) {
 if(value.isNullOrUndefined()){result.setNull();return true;}
 bool cloneError=false;
 static const JSStructuredCloneCallbacks callbacks={nullptr,nullptr,
  [](JSContext*,uint32_t,void* closure,const char*){*static_cast<bool*>(closure)=true;}};
 JSAutoStructuredCloneBuffer buffer(JS::StructuredCloneScope::SameProcess,&callbacks,&cloneError);
 if(!buffer.write(cx,value,&callbacks,&cloneError)){
  if(JS_IsThrowingOutOfMemory(cx))return false;
  if(!cloneError)return false;
  JS_ClearPendingException(cx);return PerfThrow(cx,owner,"DataCloneError");
 }
 JSAutoRealm realm(cx,owner);JS::CloneDataPolicy policy;
 if(buffer.read(cx,result,policy,&callbacks,&cloneError))return true;
 if(!cloneError||JS_IsThrowingOutOfMemory(cx))return false;
 JS_ClearPendingException(cx);return PerfThrow(cx,owner,"DataCloneError");
}
bool PerfTimestamp(JSContext* cx,JS::HandleObject owner,JS::HandleValue value,double* result) {
 if(value.isNumber()){
  *result=value.toNumber();return *result>=0||Error(cx,"Performance timestamp must be nonnegative");
 }
 JS::RootedString name(cx,value.toString());std::u16string text;
 if(!PerfText(cx,name,&text))return false;
 if(text==u"navigationStart"){*result=0;return true;}
 if(PerfReserved(cx,name))return PerfThrow(cx,owner,"InvalidAccessError");
 auto* timeline=static_cast<Timeline*>(JS::GetReservedSlot(owner,PTimeline).toPrivate());
 return ResolveTimeline(timeline,text,result)||PerfThrow(cx,owner,"SyntaxError");
}
}
