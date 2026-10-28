// Body Blob results use the original WebPlatform factory and original Headers.
#include "resource_network/body.h"
#include "resource_network/body_result.h"
#include "resource_network/body_acquire.h"
#include "resource_network/state.h"
#include "web_platform/body_factories.h"

namespace ruyijs::resource_network {
namespace {
std::string Trim(const std::string& text) {
  size_t begin=0,end=text.size();
  while (begin<end && (text[begin]==' ' || text[begin]=='\t')) ++begin;
  while (end>begin && (text[end-1]==' ' || text[end-1]=='\t')) --end;
  return text.substr(begin,end-begin);
}
}
bool MimeType(State& state,JS::HandleValue receiver,std::string* full) {
  full->clear();JS::RootedValue value(state.cx);
  if (!HeaderValue(state,receiver,"content-type",&value)) return false;
  return value.isNullOrUndefined() || Convert(state.cx,value,full);
}
std::string MimeEssence(const std::string& full) {
  const size_t end=full.find(';');
  std::string type=Trim(full.substr(0,end==std::string::npos?full.size():end));
  for (char& ch:type) if (ch>='A' && ch<='Z') ch=char(ch-'A'+'a');
  return type;
}
bool BlobMethod(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  State& state=*FunctionOwner(args);const char* iface=BodyInterface(args);
  BodyRecord* body=nullptr;bool consumed=false;
  if (!ConsumeBody(cx,args,"blob",&body,&consumed)) return false;
  state.Observe(iface,"blob",int(ApiAccess::Call));
  if (consumed) return true;
  if (body->source!=BodySource::OwnBytes) return ConsumeStreamBody(cx,args,BodyResultKind::Blob);
  std::string full;JS::RootedValue value(cx);
  if (!MimeType(state,args.thisv(),&full) || !BodyBlobValue(state,body,full,&value)) return false;
  return Settled(cx,value,args.rval());
}
bool BodyBlobValue(State& state,const BodyRecord* body,const std::string& full,JS::MutableHandleValue out) {
  JS::RootedObject factories(state.cx);
  return web_platform::BodyFactories(state.cx,state.page,&factories) &&
      BodyBlobValue(state.cx,factories,body,full,out);
}
bool BodyBlobValue(JSContext* cx,JS::HandleObject factories,const BodyRecord* body,
                    const std::string& full,JS::MutableHandleValue out) {
  JS::RootedObject blob(cx);
  if (!web_platform::BodyBlob(cx,factories,body->bytes,MimeEssence(full),&blob)) return false;
  out.setObject(*blob);return true;
}
}
