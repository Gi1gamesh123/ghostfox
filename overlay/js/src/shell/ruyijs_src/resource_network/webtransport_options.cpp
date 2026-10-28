#include "resource_network/webtransport.h"
#include "web_platform/buffer_source.h"
#include "domain/browser_environment/url_value.h"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include "js/PropertyAndElement.h"
#include "js/experimental/TypedData.h"
#include <algorithm>

namespace ruyijs::resource_network {
bool TransportBytes(JSContext* cx,JS::HandleValue value,std::string* bytes) {
  JS::RootedObject object(cx),backing(cx);bool shared=false;
  if (!web_platform::PlatformBufferSource(cx,value,&object,true)) return false;
  const bool buffer=JS::IsArrayBufferObject(object);
  backing=buffer?object.get():JS_GetArrayBufferViewBuffer(cx,object,&shared);
  if (!backing) return false;if (JS::IsDetachedArrayBufferObject(backing)) return true;
  const size_t size=buffer?JS::GetArrayBufferByteLength(object):JS_GetArrayBufferViewByteLength(object);
  JS::AutoCheckCannotGC nogc;
  const auto* data=static_cast<const char*>(buffer?JS::GetArrayBufferData(object,&shared,nogc):JS_GetArrayBufferViewData(object,&shared,nogc));
  if (size && data) bytes->assign(data,size);return true;
}
namespace {
bool Dict(JSContext* cx,JS::HandleValue value,JS::MutableHandleObject result,const char* message) {
  if (!value.isNullOrUndefined() && !value.isObject()) return Error(cx,message);
  result.set(value.isObject()?&value.toObject():nullptr);return true;
}
bool Field(JSContext* cx,JS::HandleObject object,const char* name,JS::MutableHandleValue result) {
  result.setUndefined();return !object || JS_GetProperty(cx,object,name,result);
}
bool Hashes(JSContext* cx,JS::HandleValue value,WebTransportOptions* result) {
  JS::ForOfIterator iterator(cx);if (!iterator.init(value)) return false;
  for (;;) {
    JS::RootedValue item(cx),field(cx);bool done;if (!iterator.next(&item,&done)) return false;if (done) return true;
    JS::RootedObject dictionary(cx);WebTransportHash hash;
    if (!Dict(cx,item,&dictionary,"WebTransport constructor: WebTransportHash can't be converted to a dictionary.")) return false;
    if (!Field(cx,dictionary,"algorithm",&field)) return false;
    if (field.isUndefined()) return Error(cx,"WebTransport constructor: Missing required 'algorithm' member of WebTransportHash.");
    if (!Convert(cx,field,&hash.algorithm) || !Field(cx,dictionary,"value",&field)) return false;
    if (field.isUndefined()) return Error(cx,"WebTransport constructor: Missing required 'value' member of WebTransportHash.");
    if (!TransportBytes(cx,field,&hash.bytes)) return false;
    result->hashes.push_back(std::move(hash));
  }
}
bool Protocols(JSContext* cx,JS::HandleValue value,WebTransportOptions* result) {
  JS::ForOfIterator iterator(cx);if (!iterator.init(value)) return false;
  for (;;) {JS::RootedValue item(cx);bool done;if (!iterator.next(&item,&done)) return false;if (done) return true;
    std::string protocol;if (!Convert(cx,item,&protocol)) return false;result->protocols.push_back(std::move(protocol));}
}
}
bool TransportOptions(JSContext* cx,const JS::CallArgs& args,WebTransportOptions* result) {
  auto& state=*FunctionOwner(args);if (!Convert(cx,args[0],&result->url)) return false;
  JS::RootedObject options(cx);JS::RootedValue value(cx);bool hasHashes=false;
  if (!Dict(cx,args.get(1),&options,"WebTransport constructor: Argument 2 can't be converted to a dictionary.")) return false;
  if (!Field(cx,options,"allowPooling",&value)) return false;result->allowPooling=JS::ToBoolean(value);
  if (!Field(cx,options,"congestionControl",&value)) return false;
  if (!value.isUndefined()) {
    if (!Convert(cx,value,&result->congestion)) return false;
    if (result->congestion!="default" && result->congestion!="throughput" && result->congestion!="low-latency")
      return Error(cx,("WebTransport constructor: '"+result->congestion+"' (value of 'congestionControl' member of WebTransportOptions) is not a valid value for enumeration WebTransportCongestionControl.").c_str());
  }
  if (!Field(cx,options,"protocols",&value) || (!value.isUndefined() && !Protocols(cx,value,result))) return false;
  if (!Field(cx,options,"requireUnreliable",&value)) return false;result->requireUnreliable=JS::ToBoolean(value);
  if (!Field(cx,options,"serverCertificateHashes",&value)) return false;
  hasHashes=!value.isUndefined();if (hasHashes && !Hashes(cx,value,result)) return false;
  domain::browser_environment::UrlValue url;
  if (!domain::browser_environment::UrlValue::Resolve(result->url,"",&url) || url.protocol()!="https:" || url.href().find('#')!=std::string::npos)
    return TransportThrowDom(state,"SyntaxError","WebTransport constructor: Invalid WebTransport URL");
  result->url=url.href();
  if (hasHashes && result->allowPooling) return TransportThrowDom(state,"NotSupportedError","WebTransport constructor: serverCertificateHashes not supported for non-dedicated connections");
  for (size_t i=0;i<result->protocols.size();++i) {
    const auto& p=result->protocols[i];const char* failure=nullptr;
    if (p.empty()) failure="WebTransport constructor: Protocol cannot be empty string";
    else if (std::any_of(p.begin(),p.end(),[](unsigned char c){return c<32 || c>126;})) failure="WebTransport constructor: Protocol contains characters not allowed in HTTP header fields";
    else if (p.size()>512) failure="WebTransport constructor: Protocol name too long (max 512 bytes)";
    else if (std::find(result->protocols.begin(),result->protocols.begin()+i,p)!=result->protocols.begin()+i) failure="WebTransport constructor: Duplicate protocol in list";
    if (failure) return TransportThrowDom(state,"SyntaxError",failure);
  }
  HttpContext context;if (!state.resources.ReadHttpContext(&context)) return false;
  result->origin=domain::browser_environment::UrlValue(context.source).origin();return true;
}
bool TransportSendOptions(JSContext* cx,JS::HandleObject transport,JS::HandleValue input,JS::MutableHandleValue group,int64_t* order) {
  JS::RootedObject dictionary(cx);JS::RootedValue value(cx);group.setNull();*order=0;
  if (!Dict(cx,input,&dictionary,"WebTransport send options can't be converted to a dictionary.")) return false;
  if (!Field(cx,dictionary,"sendGroup",&value)) return false;
  if (!value.isNullOrUndefined()) {
    JS::RootedObject object(cx,value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr);
    if (!object || JS::GetClass(object)!=&kTransportGroupClass) return Error(cx,"WebTransport sendGroup must implement WebTransportSendGroup");
    if (SlotObject(object,0)!=transport) return Error(cx,"sendGroup belongs to a different WebTransport");
    group.setObject(*object);
  }
  if (!Field(cx,dictionary,"sendOrder",&value)) return false;
  if (!value.isUndefined()) return JS::ToInt64(cx,value,order);return true;
}
}
