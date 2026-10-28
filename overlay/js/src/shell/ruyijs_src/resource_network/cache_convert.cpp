#include "resource_network/cache.h"
#include "resource_network/request.h"
#include "js/Conversions.h"
#include "js/String.h"
#include "domain/browser_environment/url_value.h"

namespace ruyijs::resource_network {
bool CacheName(JSContext* cx,JS::HandleValue value,std::u16string* result) {
  JS::RootedString string(cx,JS::ToString(cx,value));
  if (!string) return false;
  size_t length=JS_GetStringLength(string);
  result->resize(length);
  return JS_CopyStringChars(cx,mozilla::Range<char16_t>(result->data(),length),string);
}
bool CacheRequestInfo(State& state,JS::HandleValue value,RequestDraft* result,
                      std::string* text,bool* isRequest) {
  JSContext* cx=state.cx;
  JS::RootedObject object(cx,BodyReceiver(value));
  *isRequest=object && JS::GetClass(object)==&kRequestClass;
  if (*isRequest) {
    JS::RootedValue signal(cx);
    // Snapshot only: this deliberately never passes through RequestInput's transfer.
    if (!ReadRequest(state,object,result,&signal)) return false;
    result->body.clear();result->nullBody=true;return true;
  }
  return EncodeBodyText(cx,value,text);
}
bool CacheResolveRequest(State& state,RequestDraft* request,const std::string& text,bool isRequest,
                          const char* method,const char* iface) {
  if (isRequest) return true;
  HttpContext context;
  if (!state.resources.ReadHttpContext(&context)) return false;
  domain::browser_environment::UrlValue url;
  if (!domain::browser_environment::UrlValue::Resolve(text,context.source,&url)) {
    const std::string message=text+" is not a valid URL.";
    return CacheBindingError(state.cx,iface,method,message.c_str());
  }
  if (!url.username().empty() || !url.password().empty())
    return CacheBindingError(state.cx,iface,method,"Request URL has credentials.");
  request->url=url.href();return true;
}
bool CacheOptions(JSContext* cx,JS::HandleValue value,CacheStorageQueryDTO* query,bool storage,const char* method) {
  if (value.isNullOrUndefined()) return true;
  if (!value.isObject()) return CacheBindingError(cx,storage?"CacheStorage":"Cache",method,
      "Argument 2 can't be converted to a dictionary.");
  JS::RootedObject object(cx,&value.toObject());
  JS::RootedValue member(cx);
  struct Flag { const char* name;bool* output; };
  const Flag flags[]={{"ignoreMethod",&query->query.ignoreMethod},
      {"ignoreSearch",&query->query.ignoreSearch},{"ignoreVary",&query->query.ignoreVary}};
  for (const auto& flag:flags) {
    if (!JS_GetProperty(cx,object,flag.name,&member)) return false;
    *flag.output=JS::ToBoolean(member);
  }
  if (storage) {
    if (!JS_GetProperty(cx,object,"cacheName",&member)) return false;
    if (!member.isUndefined()) {
      query->hasName=true;
      if (!CacheName(cx,member,&query->name)) return false;
    }
  }
  return true;
}
bool CacheResponseRead(State& state,JS::HandleObject response,CacheResponseDTO* result) {
  JSContext* cx=state.cx;
  result->status=JS::GetReservedSlot(response,2).toInt32();
  result->redirected=JS::GetReservedSlot(response,6).toBoolean();
  JS::RootedValue value(cx,JS::GetReservedSlot(response,1));
  if (!HeaderPairsArgument(cx,value,&result->headers)) return false;
  auto read=[&](unsigned slot,std::string* out) {
    value=JS::GetReservedSlot(response,slot);return EncodeBodyText(cx,value,out);
  };
  const BodyRecord* body=BodyOf(response);
  result->kind=body->kind;result->nullBody=body->nullBody;
  return read(3,&result->statusText) && read(4,&result->url) && read(5,&result->type);
}
}
