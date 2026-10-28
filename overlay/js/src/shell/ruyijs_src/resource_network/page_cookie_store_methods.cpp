// The four CookieStore operations (dom/webidl/CookieStore.webidl) over the one
// cookie jar Resource Network owns. Each one converts its arguments
// (page_cookie_options.cpp), reads or writes the jar through the same
// CookieQuery/CookieAssign entry points document.cookie uses, and resolves its
// promise; a write also queues the trusted change event the jar reported.
#include "resource_network/page_cookie_store.h"
#include "resource_network/state.h"
#include "js/Array.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::resource_network {
namespace {
bool Fail(JSContext* cx, JS::MutableHandleValue rval, const std::string& error) {
  if (error.empty()) return false;  // an engine error is already pending
  return TypeErrorPromise(cx, error.c_str(), rval);
}
// One write: the document.cookie entry point (http=false), then the change event
// the jar reports. The promise resolves with undefined.
bool Write(State& s, const CookieWriteOptions& write, JS::MutableHandleValue rval) {
  JSContext* cx = s.cx;
  CookieChangeSet change;
  if (!Report(cx, CookieAssign(s.store, write.url, write.text, write.now, false, "", false,
                               &change))) return false;
  return PostCookieChange(s, change) && Settled(cx, JS::UndefinedHandleValue, rval);
}
}  // namespace

bool CookieStoreGet(State& s, const JS::CallArgs& args) {
  JSContext* cx = s.cx;
  CookieQueryOptions query;
  std::string error;
  if (!CookieQueryArguments(s, true, args, &query, &error)) return Fail(cx, args.rval(), error);
  std::vector<CookieEntry> matches;
  if (!Report(cx, CookieQuery(s.store, query.url, query.now, false, &matches))) return false;
  // A named read returns the first such cookie in the jar's order; a url-only
  // read returns the first cookie the query matched.
  const CookieEntry* found = nullptr;
  for (const auto& entry : matches)
    if (query.name.empty() || entry.name == query.name) { found = &entry; break; }
  if (!found) return Settled(cx, JS::NullHandleValue, args.rval());
  JS::RootedObject item(cx, CookieItem(cx, *found, true));
  JS::RootedValue value(cx, JS::ObjectValue(*item));
  return item && Settled(cx, value, args.rval());
}

bool CookieStoreGetAll(State& s, const JS::CallArgs& args) {
  JSContext* cx = s.cx;
  CookieQueryOptions query;
  std::string error;
  if (!CookieQueryArguments(s, false, args, &query, &error)) return Fail(cx, args.rval(), error);
  std::vector<CookieEntry> matches;
  if (!Report(cx, CookieQuery(s.store, query.url, query.now, false, &matches))) return false;
  JS::RootedObject list(cx, JS::NewArrayObject(cx, 0));
  if (!list) return false;
  uint32_t index = 0;
  for (const auto& entry : matches) {
    if (!query.name.empty() && entry.name != query.name) continue;
    JS::RootedObject item(cx, CookieItem(cx, entry, true));
    if (!item || !JS_SetElement(cx, list, index++, item)) return false;
  }
  JS::RootedValue value(cx, JS::ObjectValue(*list));
  return Settled(cx, value, args.rval());
}

bool CookieStoreSet(State& s, const JS::CallArgs& args) {
  CookieWriteOptions write;
  std::string error;
  if (!CookieSetArguments(s, args, &write, &error)) return Fail(s.cx, args.rval(), error);
  return Write(s, write, args.rval());
}

bool CookieStoreDelete(State& s, const JS::CallArgs& args) {
  CookieWriteOptions write;
  std::string error;
  if (!CookieDeleteArguments(s, args, &write, &error)) return Fail(s.cx, args.rval(), error);
  return Write(s, write, args.rval());
}
}  // namespace ruyijs::resource_network
