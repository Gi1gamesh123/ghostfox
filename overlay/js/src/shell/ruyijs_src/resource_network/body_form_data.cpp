// Original multipart parser and original WebPlatform form factories/list only.
#include "resource_network/body.h"
#include "resource_network/body_result.h"
#include "resource_network/body_acquire.h"
#include "resource_network/multipart.h"
#include "resource_network/state.h"
#include "web_platform/body_factories.h"
#include "web_platform/state.h"
#include "js/String.h"

namespace ruyijs::resource_network {
namespace {
bool AppendParts(JSContext* cx,JS::HandleObject factories,JS::HandleObject form,
                   const std::vector<MultipartPart>& parts) {
  for (const auto& part:parts) {
    JS::RootedString name(cx,JS_NewStringCopyN(cx,part.name.data(),part.name.size()));
    JS::RootedValue value(cx),key(cx);
    if (!name) return false;
    key.setString(name);
    if (part.filename.empty()) {
      JS::RootedString text(cx,JS_NewStringCopyN(cx,part.data.data(),part.data.size()));
      if (!text) return false;
      value.setString(text);
    } else {
      JS::RootedObject file(cx);
      JS::RootedString filename(cx,JS_NewStringCopyN(cx,part.filename.data(),part.filename.size()));
      if (!filename) return false;
      JS::RootedValue input(cx,JS::StringValue(filename));std::string normalized;
      if (!web_platform::Convert(cx,input,&normalized) ||
          !web_platform::BodyFile(cx,factories,part.data,part.content_type,normalized,&file)) return false;
      value.setObject(*file);
    }
    if (!web_platform::BodyFormAppend(cx,form,key,value)) return false;
  }
  return true;
}
bool AppendUrlencoded(JSContext* cx,JS::HandleObject form,const BodyRecord* body) {
  // Preserve the original Body -> URLSearchParams input conversion.
  JS::RootedString text(cx,JS_NewStringCopyN(cx,body->bytes.data(),body->bytes.size()));
  if (!text) return false;
  JS::RootedValue value(cx,JS::StringValue(text));std::string query;
  if (!web_platform::Convert(cx,value,&query)) return false;
  if (!query.empty() && query[0]=='?') query.erase(0,1);
  return web_platform::BodyFormUrlencoded(cx,form,query);
}
}
bool FormDataMethod(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  State& state=*FunctionOwner(args);const char* iface=BodyInterface(args);
  BodyRecord* body=nullptr;bool consumed=false;
  if (!ConsumeBody(cx,args,"formData",&body,&consumed)) return false;
  state.Observe(iface,"formData",int(ApiAccess::Call));
  if (consumed) return true;
  if (body->source!=BodySource::OwnBytes) return ConsumeStreamBody(cx,args,BodyResultKind::FormData);
  std::string full;JS::RootedValue value(cx);const char* parseError=nullptr;
  if (!MimeType(state,args.thisv(),&full) || !BodyFormDataValue(state,body,full,&value,&parseError)) return false;
  if (parseError) return TypeErrorPromise(cx,parseError,args.rval());
  return Settled(cx,value,args.rval());
}
bool BodyFormDataValue(State& state,const BodyRecord* body,const std::string& full,
                        JS::MutableHandleValue out,const char** parseError) {
  JS::RootedObject factories(state.cx);
  return web_platform::BodyFactories(state.cx,state.page,&factories) &&
      BodyFormDataValue(state.cx,factories,body,full,out,parseError);
}
bool BodyFormDataValue(JSContext* cx,JS::HandleObject factories,const BodyRecord* body,
                        const std::string& full,JS::MutableHandleValue out,const char** parseError) {
  *parseError=nullptr;JS::RootedObject form(cx);
  if (!web_platform::BodyFormData(cx,factories,&form)) return false;
  if (MimeEssence(full)=="multipart/form-data") {
    std::string boundary;
    if (!MultipartBoundary(full,&boundary)) {*parseError="formData requires a multipart boundary";return true;}
    std::vector<MultipartPart> parts;
    if (!ParseMultipart(body->bytes,boundary,&parts)) {*parseError="formData could not parse the multipart body";return true;}
    if (!AppendParts(cx,factories,form,parts)) return false;
  } else if (!AppendUrlencoded(cx,form,body)) return false;
  out.setObject(*form);return true;
}
}
