// Speculative script preloads (docs/owners/PAGE_LOAD.md). The
// donor's HTML parser queues nsHtml5SpeculativeLoads while it tokenizes, so a
// parser-blocking <script src> is usually already in flight when the parser
// reaches it. This product starts the same requests from the completed markup
// before parsing, from this Context's one asynchronous transport; the parser
// still executes scripts in document order and falls back to the synchronous
// fetch on a miss, so behavior never depends on a preload completing.
#include "runtime/page_load.h"
#include "runtime/runtime.h"
#include "browser_environment/environment.h"
#include "resource_network/async_http.h"
#include "execution_scheduling/realm.h"
#include "js/CharacterEncoding.h"
#include "js/Realm.h"
#include <chrono>
#include <cstdio>

namespace ruyijs {
namespace {
// The src of every classic <script> start tag (stylesheets load through the
// Styling owner's own path and are not preloaded here).
void Collect(const std::string& markup, std::vector<std::string>* urls) {
  size_t at = 0;
  while ((at = markup.find('<', at)) != std::string::npos) {
    ++at;
    size_t start = at;
    while (start < markup.size() && markup[start] == '/') ++start;
    const size_t name_end = markup.find_first_of(" \t\r\n/>", start);
    if (name_end == std::string::npos) return;
    std::string name = markup.substr(start, name_end - start);
    for (char& c : name) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    const size_t end = markup.find('>', name_end);
    if (end == std::string::npos) return;
    at = end + 1;
    if (name != "script") continue;
    const std::string tag = markup.substr(name_end, end - name_end);
    // Classic scripts only: a module or data-block type is not fetched by the
    // parser path, so preloading it would only add a request.
    if (tag.find("type=") != std::string::npos && tag.find("javascript") == std::string::npos) continue;
    const size_t src = tag.find("src=");
    if (src == std::string::npos) continue;
    size_t value = src + 4;
    if (value >= tag.size() || (tag[value] != '"' && tag[value] != '\'')) continue;
    const char quote = tag[value++];
    const size_t close = tag.find(quote, value);
    if (close == std::string::npos || close == value) continue;
    urls->push_back(tag.substr(value, close - value));
    if (urls->size() >= 64) return;
  }
}
}
// Starts one asynchronous GET per URL of `markup` the product would fetch
// anyway. A completion is stored for TakePreload; nothing is executed here.
// A document with its own request context (`frame`) preloads on its own source
// under its own keys; only a root document load clears the table (a top-level
// navigation discards every frame), so a frame's load never drops another
// document's preloads in flight.
void PageLoad::StartPreloads(JS::HandleString markup, const std::string& base,
                             resource_network::Realm* frame) {
  if (!frame) preloads_.clear();
  if (!transport_ || base.empty()) return;
  std::string text;
  if (!ToUTF8(cx_, markup, &text)) return;
  std::vector<std::string> references;
  Collect(text, &references);
  for (const std::string& reference : references) {
    std::string url;
    // One speculative request per URL: the first script element that asks
    // takes it, a repeated <script src> fetches again as the parser would.
    if (resource_network::ResolveURL(base, reference, &url)) StartPreload(url, frame);
  }
}
std::string PageLoad::PreloadKey(const resource_network::Model& model, const std::string& url) const {
  if (&model == &resources_) return url;
  char owner[24];
  std::snprintf(owner, sizeof owner, "%p ", static_cast<const void*>(&model));
  return owner + url;
}
void PageLoad::FinishPreload(void* target, resource_network::AsyncHttpResult&& result) noexcept {
  auto& slot = *static_cast<Preload*>(target);
  slot.done = true;
  slot.ok = result.outcome == resource_network::AsyncHttpOutcome::Http && result.ok &&
            result.response.status >= 200 && result.response.status <= 299;
  slot.http = std::move(result.response);
}
bool PageLoad::StartPreload(const std::string& url, resource_network::Realm* frame) {
  if (!transport_) return false;
  const std::string key = frame ? PreloadKey(frame->Resources(), url) : url;
  if (preloads_.count(key)) return false;
  std::shared_ptr<Preload> slot = std::make_shared<Preload>();
  resource_network::HttpOptions options;
  options.initiator = "script";
  if (frame) {
    // The frame's own source: its Model builds the request (Referer,
    // Sec-Fetch-Site, cookie partition) and records its Resource Timing.
    std::shared_ptr<resource_network::AsyncHttpSource> source = frame->AsyncSource();
    resource_network::AsyncHttpTicket ticket;
    const std::string method = "GET", body;
    const resource_network::HeaderPairs headers;
    if (!source || resource_network::StartAsyncHttp(source,
            {method, url, headers, body, resource_network::RedirectMode::Follow, options},
            {slot, FinishPreload}, &ticket).code != resource_network::ErrorCode::None)
      return false;
    preloads_[key] = slot;
    return true;
  }
  preloads_[key] = slot;
  transport_->Start("GET", url, {}, "", resource_network::RedirectMode::Follow, options,
      [slot](bool ok, resource_network::HttpResult&& result) {
        slot->done = true;
        slot->ok = ok && result.status >= 200 && result.status <= 299;
        slot->http = std::move(result);
      });
  return true;
}
bool PageLoad::PreloadPending(const std::string& key) const {
  auto found = preloads_.find(key);
  return found != preloads_.end() && !found->second->done;
}
// The preloaded body of `url`, waiting for it when the request is still in
// flight; false when no preload exists or it ended in a non-2xx response. A
// preload that ended in failure sets |failed|: it was this element's fetch
// (Firefox reports its error, it does not issue the request again).
bool PageLoad::TakePreload(const std::string& key, resource_network::HttpResult* http, bool* failed) {
  auto found = preloads_.find(key);
  if (found == preloads_.end()) return false;
  std::shared_ptr<Preload> slot = found->second;
  preloads_.erase(found);
  // A completion may belong to another script or an image. Keep consuming
  // external work until this response arrives; treating an unrelated
  // completion as a miss would issue a second GET for the in-flight script.
  // Cap the wait at 8 seconds: on platforms where the async transport's
  // completion signal never fires (Wine), the miss lets Source() fall back
  // to its synchronous PerformHttp path instead of blocking forever.
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
  while (!slot->done && scheduling_.RunExternalTask()) {
    if (std::chrono::steady_clock::now() > deadline) break;
  }
  if (!slot->done) return false;
  if (!slot->ok) {
    if (failed) *failed = true;
    return false;
  }
  *http = std::move(slot->http);
  return true;
}
}
