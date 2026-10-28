#ifndef RUYIJS_RESOURCE_WORKER_NETWORK_WIRE_H
#define RUYIJS_RESOURCE_WORKER_NETWORK_WIRE_H
// The bounded DTO a Worker's fetch crosses the Worker service channel with: the
// child sends one whole request, the parent's one Resource Network owner runs
// it (Cookie, NetLog, redirects, Necko) and returns one HttpResult. The context
// request returns the parent browsing context's HttpContext, which the child's
// request conversion resolves URLs and referrers against.
#include "resource_network/page_http.h"
#include <cstdint>
#include <string>
#include <string_view>

namespace ruyijs::resource_network::worker_network_wire {
// The first byte separates these frames from the object URL frames ("OU1").
inline constexpr char kRequest = 'N', kContext = 'C';
inline constexpr char kReplyOk = 'O', kReplyFailed = 'F';
inline constexpr size_t kTextLimit = 64 * 1024, kListLimit = 256;
class Writer {
 public:
  explicit Writer(std::string* out) : out_(*out) {}
  void Number(uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) out_.push_back(char(value >> (i * 8)));
  }
  void Wide(uint64_t value) { Number(uint32_t(value)); Number(uint32_t(value >> 32)); }
  bool Text(std::string_view text, size_t limit = kTextLimit) {
    if (text.size() > limit) return false;
    Number(uint32_t(text.size())); out_.append(text); return true;
  }
  bool Pairs(const HeaderPairs& pairs) {
    if (pairs.size() > Limits::header_fields) return false;
    Number(uint32_t(pairs.size()));
    for (const auto& pair : pairs) if (!Text(pair.first) || !Text(pair.second)) return false;
    return true;
  }
 private:
  std::string& out_;
};
class Reader {
 public:
  explicit Reader(std::string_view data) : data_(data) {}
  bool Number(uint32_t* value) {
    if (data_.size() - at_ < 4) return false;
    *value = 0;
    for (unsigned i = 0; i < 4; ++i)
      *value |= uint32_t(static_cast<unsigned char>(data_[at_ + i])) << (i * 8);
    at_ += 4; return true;
  }
  bool Wide(uint64_t* value) {
    uint32_t low = 0, high = 0;
    if (!Number(&low) || !Number(&high)) return false;
    *value = uint64_t(low) | (uint64_t(high) << 32); return true;
  }
  bool Text(std::string* text, size_t limit = kTextLimit) {
    uint32_t size = 0;
    if (!Number(&size) || size > limit || data_.size() - at_ < size) return false;
    text->assign(data_.substr(at_, size)); at_ += size; return true;
  }
  bool Pairs(HeaderPairs* pairs) {
    uint32_t count = 0;
    if (!Number(&count) || count > Limits::header_fields) return false;
    for (uint32_t i = 0; i < count; ++i) {
      std::string name, value;
      if (!Text(&name) || !Text(&value)) return false;
      pairs->emplace_back(std::move(name), std::move(value));
    }
    return true;
  }
  bool Done() const { return at_ == data_.size(); }
 private:
  std::string_view data_;
  size_t at_ = 0;
};
struct Request {
  std::string method, url, body;
  HeaderPairs headers;
  RedirectMode redirect = RedirectMode::Follow;
  HttpOptions options;
};
inline bool EncodeRequest(const Request& r, std::string* out) {
  out->assign(1, kRequest);
  Writer w(out);
  w.Number(uint32_t(r.redirect)); w.Number(uint32_t(r.options.credentials));
  w.Number(uint32_t(r.options.cors));
  return w.Text(r.method) && w.Text(r.url) && w.Pairs(r.headers) &&
         w.Text(r.body, Limits::body_bytes) && w.Text(r.options.source) &&
         w.Text(r.options.referrer) && w.Text(r.options.referrerPolicy) && w.Text(r.options.mode);
}
// `data` excludes the leading kRequest byte. The initiator is always "fetch":
// a Worker crosses this channel only from its fetch entry.
inline bool DecodeRequest(std::string_view data, Request* r) {
  Reader in(data);
  uint32_t redirect = 0, credentials = 0, cors = 0;
  if (!in.Number(&redirect) || !in.Number(&credentials) || !in.Number(&cors) ||
      redirect > uint32_t(RedirectMode::Manual) || credentials > uint32_t(Credentials::Include) ||
      cors > 1 || !in.Text(&r->method) || !in.Text(&r->url) || !in.Pairs(&r->headers) ||
      !in.Text(&r->body, Limits::body_bytes) || !in.Text(&r->options.source) ||
      !in.Text(&r->options.referrer) || !in.Text(&r->options.referrerPolicy) ||
      !in.Text(&r->options.mode) || !in.Done()) return false;
  r->redirect = RedirectMode(redirect);
  r->options.credentials = Credentials(credentials);
  r->options.cors = cors != 0;
  r->options.initiator = "fetch";
  return true;
}
inline bool EncodeResult(bool ok, const HttpResult& h, std::string* out) {
  out->assign(1, ok ? kReplyOk : kReplyFailed);
  Writer w(out);
  w.Number(uint32_t(h.status)); w.Number(uint32_t(h.kind));
  w.Number(uint32_t(h.redirected) | uint32_t(h.opaqueRedirect) << 1 |
           uint32_t(h.transferred) << 2 | uint32_t(h.crossOrigin) << 3);
  w.Wide(h.encodedBytes); w.Wide(h.decodedBytes);
  return w.Text(h.error) && w.Text(h.statusText) && w.Pairs(h.headers) &&
         w.Text(h.body, Limits::body_bytes) && w.Text(h.finalURL) && w.Text(h.protocol);
}
inline bool DecodeResult(std::string_view data, bool* ok, HttpResult* h) {
  if (data.empty() || (data[0] != kReplyOk && data[0] != kReplyFailed)) return false;
  *ok = data[0] == kReplyOk;
  Reader in(data.substr(1));
  uint32_t status = 0, kind = 0, flags = 0;
  if (!in.Number(&status) || !in.Number(&kind) || !in.Number(&flags) || kind > 1 || status > 999 ||
      !in.Wide(&h->encodedBytes) || !in.Wide(&h->decodedBytes) || !in.Text(&h->error) ||
      !in.Text(&h->statusText) || !in.Pairs(&h->headers) || !in.Text(&h->body, Limits::body_bytes) ||
      !in.Text(&h->finalURL) || !in.Text(&h->protocol) || !in.Done()) return false;
  h->status = int(status); h->kind = BodyKind(kind);
  h->redirected = flags & 1; h->opaqueRedirect = flags & 2;
  h->transferred = flags & 4; h->crossOrigin = flags & 8;
  return true;
}
inline bool EncodeContext(const HttpContext& c, std::string* out) {
  out->assign(1, kReplyOk);
  Writer w(out);
  if (c.languages.size() > kListLimit || !w.Text(c.source) || !w.Text(c.site) || !w.Text(c.userAgent))
    return false;
  w.Number(uint32_t(c.languages.size()));
  for (const auto& language : c.languages) if (!w.Text(language)) return false;
  return true;
}
inline bool DecodeContext(std::string_view data, HttpContext* c) {
  if (data.empty() || data[0] != kReplyOk) return false;
  Reader in(data.substr(1));
  uint32_t count = 0;
  if (!in.Text(&c->source) || !in.Text(&c->site) || !in.Text(&c->userAgent) ||
      !in.Number(&count) || count > kListLimit) return false;
  for (uint32_t i = 0; i < count; ++i) {
    std::string language;
    if (!in.Text(&language)) return false;
    c->languages.push_back(std::move(language));
  }
  return in.Done();
}
}
#endif
