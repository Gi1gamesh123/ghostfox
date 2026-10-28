#include "web_indexeddb/state.h"
#include "runtime/runtime.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/StableStringChars.h"
#include "js/String.h"
namespace ruyijs::web_indexeddb {
// A member's receiver: a brand failure reports Firefox's text for `iface`
// (the class's interface unless given).
State* Receiver(JSContext* cx,const JS::CallArgs& args,const JSClass* clasp,JS::MutableHandleObject out,
                const char* iface) {
  JS::HandleValue value=args.thisv();
  if (!value.isObject() || JS::GetClass(&value.toObject())!=clasp) {
    WebKit::FailBrand(cx,args,iface?iface:clasp->name);return nullptr;
  }
  out.set(&value.toObject());
  return static_cast<State*>(JS::GetReservedSlot(out,0).toPrivate());
}
bool Text(JSContext* cx,JS::HandleValue value,std::u16string* out) {
  JS::RootedString text(cx,JS::ToString(cx,value));
  if (!text) return false;
  if (JS_GetStringLength(text)>4096) return WebKit::Fail(cx,"IndexedDB name exceeds product limit");
  JS::AutoStableStringChars chars(cx);
  if (!chars.initTwoByte(cx,text)) return false;
  out->assign(chars.twoByteChars(),chars.length());return true;
}
bool String(JSContext* cx,const std::u16string& text,JS::MutableHandleValue result) {
  JSString* string=JS_NewUCStringCopyN(cx,text.data(),text.size());
  if (!string) return false;
  result.setString(string);return true;
}
bool ErrorValue(State& s,const char* name,JS::MutableHandleValue result) {
  JS::RootedString text(s.cx,JS_NewStringCopyZ(s.cx,name));
  JS::RootedValueArray<2> args(s.cx);JS::RootedObject error(s.cx);
  if (!text) return false;
  args[0].setString(text);args[1].setString(text);
  if (!JS::Construct(s.cx,s.exceptionCtor,args,&error)) return false;
  result.setObject(*error);return true;
}
bool Throw(State& s,const char* name) {
  JS::RootedValue error(s.cx);
  if (!ErrorValue(s,name,&error)) return false;
  JS_SetPendingException(s.cx,error);return false;
}
bool Origin(State& s,std::string* result) {
  JS::RootedValue href(s.cx);
  if (!s.environment.URL(&href) || !href.isString()) return false;
  JS::RootedString text(s.cx,href.toString());std::string url;
  if (!ToUTF8(s.cx,text,&url)) return false;
  if ((url=="about:blank" || url=="about:srcdoc") && s.parent) return Origin(*s.parent,result);
  if (!OriginOf(url,result)) {JS_ReportOutOfMemory(s.cx);return false;}
  return *result!="null" || Throw(s,"SecurityError");
}
}
