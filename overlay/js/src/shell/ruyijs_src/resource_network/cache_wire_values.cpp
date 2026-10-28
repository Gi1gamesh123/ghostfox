#include "resource_network/cache_wire.h"

namespace ruyijs::resource_network::cache_wire {
bool Text16(Writer& writer,std::u16string_view value) {
  if (value.size()>32768) return false;
  writer.Number(uint32_t(value.size()));
  for (char16_t c:value) writer.Number(uint32_t(c));
  return true;
}
bool Text16(Reader& reader,std::u16string* value) {
  uint32_t size=0,c=0;
  if (!reader.Number(&size) || size>32768) return false;
  value->reserve(size);
  for (uint32_t i=0;i<size;++i) {
    if (!reader.Number(&c) || c>65535) return false;
    value->push_back(char16_t(c));
  }
  return true;
}
bool Request(Writer& w,const RequestDraft& r) {
  w.Number(uint32_t(r.keepalive));
  return w.Text(r.method) && w.Text(r.url) && w.Pairs(r.headers) &&
      w.Text(r.mode) && w.Text(r.credentials) && w.Text(r.cache) && w.Text(r.redirect) &&
      w.Text(r.referrer) && w.Text(r.referrer_policy) && Text16(w,r.integrity);
}
bool Request(Reader& r,RequestDraft* q) {
  uint32_t keepalive=0;
  if (!r.Number(&keepalive) || keepalive>1) return false;
  q->keepalive=keepalive!=0;
  return r.Text(&q->method) && r.Text(&q->url) && r.Pairs(&q->headers) &&
      r.Text(&q->mode) && r.Text(&q->credentials) && r.Text(&q->cache) && r.Text(&q->redirect) &&
      r.Text(&q->referrer) && r.Text(&q->referrer_policy) && Text16(r,&q->integrity);
}
bool Response(Writer& w,const CacheResponseDTO& r) {
  w.Number(uint32_t(r.status));w.Number(uint32_t(r.kind));
  w.Number(uint32_t(r.redirected)|(uint32_t(r.nullBody)<<1));
  return w.Text(r.statusText) && w.Text(r.url) && w.Text(r.type) && w.Pairs(r.headers) &&
      w.Text(r.body,Limits::body_bytes);
}
bool Response(Reader& r,CacheResponseDTO* q) {
  uint32_t status=0,kind=0,flags=0;
  if (!r.Number(&status) || status>999 || !r.Number(&kind) || kind>1 ||
      !r.Number(&flags) || flags>3) return false;
  q->status=int(status);q->kind=BodyKind(kind);q->redirected=flags&1;q->nullBody=flags&2;
  return r.Text(&q->statusText) && r.Text(&q->url) && r.Text(&q->type) && r.Pairs(&q->headers) &&
      r.Text(&q->body,Limits::body_bytes);
}
}
