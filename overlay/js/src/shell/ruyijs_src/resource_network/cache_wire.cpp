#include "resource_network/cache_wire.h"

namespace ruyijs::resource_network {
namespace {
bool Query(cache_wire::Writer& w,const CacheStorageQueryDTO& q) {
  w.Number(uint32_t(q.query.hasRequest)|(uint32_t(q.query.ignoreMethod)<<1)|
      (uint32_t(q.query.ignoreSearch)<<2)|(uint32_t(q.query.ignoreVary)<<3)|(uint32_t(q.hasName)<<4));
  w.Number(uint32_t(q.query.kind));
  return cache_wire::Request(w,q.query.request) && cache_wire::Text16(w,q.name);
}
bool Query(cache_wire::Reader& r,CacheStorageQueryDTO* q) {
  uint32_t flags=0,kind=0;
  if (!r.Number(&flags) || flags>31 || !r.Number(&kind) || kind>2) return false;
  q->query.hasRequest=flags&1;q->query.ignoreMethod=flags&2;q->query.ignoreSearch=flags&4;
  q->query.ignoreVary=flags&8;q->hasName=flags&16;q->query.kind=CacheResultKind(kind);
  return cache_wire::Request(r,&q->query.request) && cache_wire::Text16(r,&q->name);
}
}
bool EncodeCacheRequest(const CacheWireRequest& request,std::string* out) {
  if (request.pairs.size()>256 || request.releases.size()>10000) return false;
  out->assign("K1");cache_wire::Writer w(out);
  w.Number(uint32_t(request.op));w.Wide(request.handle);
  w.Number(uint32_t(request.releases.size()));
  for (auto id:request.releases) w.Wide(id);
  if (!cache_wire::Text16(w,request.name) || !Query(w,request.query)) return false;
  w.Number(uint32_t(request.pairs.size()));
  for (const auto& pair:request.pairs)
    if (!cache_wire::Request(w,pair.request) || !cache_wire::Response(w,pair.response)) return false;
  return true;
}
bool DecodeCacheRequest(std::string_view text,CacheWireRequest* request) {
  if (text.substr(0,2)!="K1") return false;
  cache_wire::Reader r(text.substr(2));uint32_t op=0,count=0;
  if (!r.Number(&op) || op>uint32_t(CacheWireOp::Delete) || !r.Wide(&request->handle) ||
      !r.Number(&count) || count>10000) return false;
  request->op=CacheWireOp(op);
  for (uint32_t i=0;i<count;++i) {uint64_t id=0;if (!r.Wide(&id)) return false;request->releases.push_back(id);}
  if (!cache_wire::Text16(r,&request->name) || !Query(r,&request->query) || !r.Number(&count) || count>256) return false;
  request->pairs.resize(count);
  for (auto& pair:request->pairs)
    if (!cache_wire::Request(r,&pair.request) || !cache_wire::Response(r,&pair.response)) return false;
  return r.Done();
}
bool EncodeCacheReply(const CacheWireReply& reply,std::string* out) {
  if (reply.names.size()>10000 || reply.results.requests.size()>10000 || reply.results.responses.size()>10000) return false;
  out->assign("KR");cache_wire::Writer w(out);
  w.Number(uint32_t(reply.code));w.Wide(reply.handle);w.Number(uint32_t(reply.flag));
  w.Number(uint32_t(reply.names.size()));
  for (const auto& name:reply.names) if (!cache_wire::Text16(w,name)) return false;
  w.Number(uint32_t(reply.results.requests.size()));
  for (const auto& q:reply.results.requests) if (!cache_wire::Request(w,q)) return false;
  w.Number(uint32_t(reply.results.responses.size()));
  for (const auto& q:reply.results.responses) if (!cache_wire::Response(w,q)) return false;
  return true;
}
bool DecodeCacheReply(std::string_view text,CacheWireReply* reply) {
  if (text.substr(0,2)!="KR") return false;
  cache_wire::Reader r(text.substr(2));uint32_t code=0,flag=0,count=0;
  if (!r.Number(&code) || code>uint32_t(ErrorCode::InvalidState) || !r.Wide(&reply->handle) ||
      !r.Number(&flag) || flag>1 || !r.Number(&count) || count>10000) return false;
  reply->code=ErrorCode(code);reply->flag=flag!=0;reply->names.resize(count);
  for (auto& name:reply->names) if (!cache_wire::Text16(r,&name)) return false;
  if (!r.Number(&count) || count>10000) return false;reply->results.requests.resize(count);
  for (auto& q:reply->results.requests) if (!cache_wire::Request(r,&q)) return false;
  if (!r.Number(&count) || count>10000) return false;reply->results.responses.resize(count);
  for (auto& q:reply->results.responses) if (!cache_wire::Response(r,&q)) return false;
  return r.Done();
}
}
