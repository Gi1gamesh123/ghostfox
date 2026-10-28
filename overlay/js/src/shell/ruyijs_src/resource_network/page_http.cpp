#include "resource_network/page_http.h"
#include "resource_network/http_exchange.h"
#include "resource_network/necko_http.h"
#include "resource_network/model_storage.h"
#include "domain/browser_environment/url_value.h"
#include "domain/resource_network/data_url.h"
#include "domain/resource_network/request_defaults.h"
#include <chrono>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifdef _WIN32
#include "runtime/ghostfox_port.h"
namespace ruyijs { using ::GetTickCount64; }
#else
#include "runtime/ghostfox_port.h"
namespace ruyijs {
inline Ticks GetTickCount64() { return TicksNow(); }
using ULONGLONG = unsigned long long;
}
#include <fcntl.h>
#include <unistd.h>
#endif


namespace ruyijs::resource_network {
namespace {
using UrlValue = ruyijs::domain::browser_environment::UrlValue;
namespace domain = ruyijs::domain::resource_network;
bool Failure(HttpResult* result, const char* message) {
  *result = HttpResult{};
  result->error = message;
  return false;
}
}
bool ResolveURL(std::string_view base, std::string_view input, std::string* url) {
  UrlValue resolved;
  if (!UrlValue::Resolve(std::string(input), std::string(base), &resolved)) return false;
  *url = resolved.href();
  return true;
}

// The hop part of the former PerformHttp loop: defaults, Cookie, the NetLog
// record, and a hop that needs no transport (blob: URL, resource directory).
bool HttpExchange::Prepare(HttpResult* local) {
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started_).count();
  if (elapsed >= 20000) { Fail("HTTP redirect chain timed out"); return false; }
  remaining_ = uint32_t(20000 - elapsed);
  crossOriginTainted_ = crossOriginTainted_ || source_.origin() != address_.origin();
  outgoing_ = headers_;
  domain::RequestDefaults defaults{context_.source,address_.href(),context_.userAgent,referrer_,policy_,
    options_.initiator,options_.mode,method_,context_.languages,options_.navigation,options_.cors,redirectSite_,
    context_.site,options_.credentials==Credentials::Include,options_.urgent,options_.userActivated};
  referrer_=domain::DefaultReferrer(defaults);
  domain::AddRequestDefaults(defaults,outgoing_);
  credentials_=options_.credentials==Credentials::Include||
    (options_.credentials==Credentials::SameOrigin&&!credentialsTainted_&&source_.origin()==address_.origin());
  if(credentials_&&model_.Cookies()) {
    std::string cookie;
    auto status=CookieJoined(model_.Cookies(),address_.href(),context_.now,true,&cookie,
        context_.site,options_.navigation,method_=="GET"||method_=="HEAD");
    if(status.code!=ErrorCode::None){Fail(status.message);return false;}
    if(!cookie.empty())outgoing_.emplace_back("Cookie",std::move(cookie));
  }
  hopStart_=observe_?model_.HttpNow():0;
  if(observe_) {
    hopTick_=double(ruyijs::TicksNow());
  }
  url_ = address_.href();
  ResourceSnapshot stored;
  bool found = false;
  const bool objectUrl = url_.compare(0, 5, "blob:") == 0;
  const bool dataUrl = url_.compare(0, 5, "data:") == 0;
  const auto status = model_.Record(method_, url_, body_, &stored, &found, &record_,
                                    !objectUrl && !dataUrl);
  if (status.code != ErrorCode::None) { Failure(local, status.message); return false; }
  model_.RecordCookies(record_,outgoing_);
  if (objectUrl) {
    std::string type;
    if (method_ != "GET" || !model_.ReadObjectUrl(url_, &local->body, &type))
      { Failure(local, "Blob URL request failed"); return false; }
    local->status = 200; local->statusText = "OK"; local->kind = BodyKind::Bytes;
    local->headers = {{"content-type", std::move(type)},
                      {"content-length", std::to_string(local->body.size())}};
    model_.RecordResponse(record_, 200);
    return false;
  }
  // Fetch "scheme fetch" for data: the data URL processor's body and MIME type, no
  // network and no cookies; a URL it cannot process is a network error.
  if (dataUrl) {
    domain::DataUrl parsed;
    if (!domain::ProcessDataUrl(url_, &parsed)) { Failure(local, "Invalid data: URL"); return false; }
    local->status = 200; local->statusText = "OK"; local->kind = BodyKind::Bytes;
    local->headers = {{"content-type", std::move(parsed.mimeType)}};
    local->body = std::move(parsed.body);
    local->encodedBytes = local->decodedBytes = local->body.size();
    model_.RecordResponse(record_, 200);
    return false;
  }
  // A full observation log does not prevent the original request path from running.
  if (found) {
    local->status = stored.status;
    local->body = std::move(stored.body);
    local->headers = std::move(stored.headers);
    local->kind = stored.kind;
    local->encodedBytes=local->decodedBytes=local->body.size();
    return false;
  }
  return true;
}

HttpExchange::HttpExchange(Model& model, const std::string& method, const std::string& url,
                           const HeaderPairs& headers, const std::string& body, RedirectMode mode,
                           const HttpOptions& options)
    : model_(model), mode_(mode), options_(options), initialURL_(url),
      method_(method), body_(body), headers_(headers) {
  if (!UrlValue::Resolve(initialURL_, "", &address_)) { Fail("Invalid request URL"); return; }
  address_.set_hash("");
  if(!model_.ReadHttpContext(&context_)){Fail("Request environment unavailable");return;}
  if(!options_.source.empty())context_.source=options_.source;
  if(!options_.site.empty())context_.site=options_.site;
  if(options_.browserInitiated){context_.source.clear();context_.site.clear();}
  source_ = UrlValue(context_.source);
  referrer_=context_.source;policy_=options_.referrerPolicy;
  if(options_.referrer!="about:client") {
    if(options_.referrer.empty())referrer_.clear();
    else {UrlValue explicitURL;
      if(!UrlValue::Resolve(options_.referrer,context_.source,&explicitURL)||explicitURL.origin()!=source_.origin())referrer_.clear();
      else referrer_=explicitURL.href();}
  }
  observe_=model_.HasHttpObservation()&&*options_.initiator;
}
bool HttpExchange::Record() {
  return !observe_||model_.ObserveHttp({initialURL_,context_,options_,result_,timings_});
}
void HttpExchange::Fail(const char* message) {
  Failure(&result_, message);
  model_.RecordError(record_, result_.error);
  finished_ = true; ok_ = false;
}
// A failed hop keeps its own error result, as the former RequestHop failure did.
void HttpExchange::Finish(bool ok, HttpResult&& response) {
  result_ = std::move(response);
  if(!ok&&observe_){timings_.push_back({url_,{},hopStart_,model_.HttpNow()});Record();}
  finished_ = true;
  ok_ = ok && Record();
}
bool HttpExchange::Next() {
  while (!finished_) {
    HttpResult local;
    if (Prepare(&local)) return true;
    if (finished_) break;
    if (!local.error.empty()) Finish(false, std::move(local));
    else Complete(std::move(local));
  }
  return false;
}
void HttpExchange::Deliver(bool ok, HttpResult&& response) {
  model_.RecordResponse(record_, ok ? response.status : 0);
  if (!ok) {
    model_.RecordError(record_, response.error.empty() ? "Network request failed" : response.error);
    Finish(false, std::move(response));
  }
  else Complete(std::move(response));
}
// The response part of the former loop: Set-Cookie, timing, redirect policy.
void HttpExchange::Complete(HttpResult&& response) {
  if(observe_) {
    const double end=model_.HttpNow();
    auto offset=[&](double tick) {return tick>0?std::clamp(hopStart_+tick-hopTick_,hopStart_,end):0;};
    timings_.push_back({url_,response.headers,hopStart_,end,offset(response.requestTick),
                       offset(response.responseTick),offset(response.headersTick)});
  }
  response.finalURL = url_;
  response.crossOrigin = crossOriginTainted_;
  response.referrer = referrer_;
  response.redirected = redirects_ != 0;
  HttpContext completed;
  if(!model_.ReadHttpContext(&completed)){Fail("Request environment unavailable");return;}
  context_.now=completed.now;
  if(credentials_&&model_.Cookies())for(const auto& h:response.headers)
    if(domain::lower_header_name(h.first)=="set-cookie") {
      auto status=CookieAssign(model_.Cookies(),url_,h.second,context_.now,true,context_.site,options_.navigation);
      if(status.code!=ErrorCode::None){Fail(status.message);return;}
    }
  if (!domain::is_redirect_status(response.status)) { Finish(true, std::move(response)); return; }
  if (mode_ == RedirectMode::Error) { Fail("HTTP redirect rejected by request mode"); return; }
  if (mode_ == RedirectMode::Manual) {
    HttpResult opaque;
    opaque.finalURL = url_; opaque.kind = BodyKind::Bytes;
    opaque.opaqueRedirect = true;
    Finish(true, std::move(opaque)); return;
  }
  const std::string* location = nullptr;
  for (const auto& field : response.headers)
    if (domain::lower_header_name(field.first) == "location") { location = &field.second; break; }
  if (!location) { Finish(true, std::move(response)); return; }
  if (redirects_ >= domain::kRedirectLimit) { Fail("HTTP redirect limit exceeded"); return; }
  UrlValue next;
  if (!UrlValue::Resolve(*location, address_.href(), &next) ||
      (next.protocol() != "http:" && next.protocol() != "https:"))
    { Fail("Invalid HTTP redirect URL"); return; }
  const bool discardBody = domain::redirect_changes_to_get(response.status, method_);
  for(const auto& field:response.headers)if(domain::lower_header_name(field.first)=="referrer-policy")
    domain::ReadReferrerPolicy(field.second,&policy_);
  auto site=domain::RequestSite(context_.source,address_.href());
  if(redirectSite_!="cross-site"&&(site=="cross-site"||redirectSite_.empty()))redirectSite_=site;
  if(address_.origin()!=next.origin())credentialsTainted_=true;
  domain::redirect_headers(headers_, discardBody, address_.origin() != next.origin());
  if (discardBody) { method_ = "GET"; body_.clear(); }
  next.set_hash("");
  address_ = std::move(next);
  ++redirects_;
}

bool PerformHttp(Model& model, const std::string& method, const std::string& url,
                 const HeaderPairs& headers, const std::string& body,
                 HttpResult* result, RedirectMode mode, const HttpOptions& options) {
  if (model.HasRemoteHttp()) {
    bool ok = false;
    *result = HttpResult{};
    if (!model.RemoteHttp({method, url, headers, body, mode, options}, result, &ok)) {
      *result = HttpResult{};
      result->error = "Worker network channel failed";
      return false;
    }
    return ok;
  }
  HttpExchange exchange(model, method, url, headers, body, mode, options);
  while (exchange.Next()) {
    HttpResult response;
    const bool ok = NeckoHttp(exchange.method(), exchange.url(), exchange.headers(),
                              exchange.body(), &response, exchange.remaining());
    exchange.Deliver(ok, std::move(response));
  }
  *result = std::move(exchange.result());
  return exchange.ok();
}
}
