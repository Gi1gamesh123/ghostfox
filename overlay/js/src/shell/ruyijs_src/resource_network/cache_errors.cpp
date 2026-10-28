#include "resource_network/cache.h"
#include "resource_network/abort.h"
#include "js/ErrorReport.h"
#include "js/Promise.h"
#include <cstring>

namespace ruyijs::resource_network {
namespace {
const JSErrorFormatString* Format(void* data,unsigned) {
  return static_cast<const JSErrorFormatString*>(data);
}
}
bool CacheBindingError(JSContext* cx,const char* iface,const char* method,const char* detail) {
  JSErrorFormatString format{"RUYIJS_CACHE_BINDING","{0}.{1}: {2}",3,JSEXN_TYPEERR};
  JS_ReportErrorNumberASCII(cx,Format,&format,0,iface,method,detail);return false;
}
bool CacheRejectPending(JSContext* cx,JS::MutableHandleValue result) {
  JS::RootedValue error(cx);
  if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,&error)) return false;
  JS_ClearPendingException(cx);return Rejected(cx,error,result);
}
bool CacheRejectPromise(JSContext* cx,JS::HandleObject promise) {
  JS::RootedValue error(cx);
  if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,&error)) return false;
  JS_ClearPendingException(cx);return JS::RejectPromise(cx,promise,error);
}
bool CacheReport(JSContext* cx,Status status) {
  if (status.code==ErrorCode::None) return true;
  return Report(cx,status);
}
bool CacheValidateRequest(JSContext* cx,const RequestDraft& request,const char* method) {
  if (request.method!="GET") {
    const std::string message="Invalid request method "+request.method+".";
    return CacheBindingError(cx,"Cache",method,message.c_str());
  }
  if (request.url.compare(0,7,"http://") && request.url.compare(0,8,"https://")) {
    const std::string message="Request URL "+request.url+" must be either http:// or https://.";
    return CacheBindingError(cx,"Cache",method,message.c_str());
  }
  return true;
}
bool CacheValidateResponse(JSContext* cx,const CacheResponseDTO& response,const char* method) {
  if (response.status==206) {
    const std::string message="Cache got "+response.type+" response with bad status 206 while trying to add request "+response.url;
    return CacheBindingError(cx,"Cache",method,message.c_str());
  }
  for (const auto& field:response.headers) {
    std::string name=field.first;
    for (char& c:name) if (c>='A' && c<='Z') c+='a'-'A';
    if (name!="vary") continue;
    size_t begin=0;
    while (begin<=field.second.size()) {
      size_t end=field.second.find(',',begin);
      if (end==std::string::npos) end=field.second.size();
      std::string_view part(field.second.data()+begin,end-begin);
      while (!part.empty() && (part.front()==' ' || part.front()=='\t')) part.remove_prefix(1);
      while (!part.empty() && (part.back()==' ' || part.back()=='\t')) part.remove_suffix(1);
      if (part=="*") return CacheBindingError(cx,"Cache",method,"Invalid Response object with a 'Vary: *' header.");
      if (end==field.second.size()) break;
      begin=end+1;
    }
  }
  return true;
}
State* CacheState(JSContext* cx,JS::HandleObject self) {
  JS::RootedObject factories(cx,SlotObject(self,CFactories));
  return StreamFactoryState(cx,factories);
}
}
