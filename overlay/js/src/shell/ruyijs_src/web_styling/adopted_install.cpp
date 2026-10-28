// The original Styling installer and receiver realm's native ObservableArray.
#include "web_styling/adopted.h"
#include "web_styling/realm.h"
#include "js/Array.h"
#include "js/ForOfIterator.h"
#include "js/Realm.h"

namespace ruyijs::web_styling {
namespace {
bool Resolve(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleObject out) {
  State& state = *detail::Self(args);
  JS::RootedValue array(cx);
  if (!state.adoptedResolve) return WebKit::Fail(cx, "Adopted CSS receiver port is not installed");
  if (!state.adoptedResolve(state.adoptedContext, args.thisv(), &array, WebKit::Slot(args) == 1)) return false;
  out.set(js::CheckedUnwrapStatic(&array.toObject())); return !!out;
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject proxy(cx), rows(cx);
  if (!Resolve(cx, args, &proxy)) return false;
  {
    JSAutoRealm realm(cx, AdoptedOwnerOf(proxy).page);
    if (!AdoptedBacking(cx, proxy, &rows)) return false;
    args.rval().setObject(*proxy);
  }
  return JS_WrapValue(cx, args.rval());
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject proxy(cx), sheet(cx);
  if (!Resolve(cx, args, &proxy)) return false;
  if (!args.get(0).isObject()) return WebKit::Fail(cx, "adoptedStyleSheets requires an iterable object");
  // Convert the full sequence before clearing, including self-assignment.
  JS::ForOfIterator iterator(cx);
  if (!iterator.init(args[0])) return false;
  JS::RootedValue value(cx);
  JS::RootedValueVector converted(cx);
  bool done;
  while (true) {
    if (!iterator.next(&value, &done)) return false;
    if (done) break;
    if (!CssReceiver(cx, value, &kSheetClass, &sheet)) { iterator.closeThrow(); return false; }
    if (!converted.append(JS::ObjectValue(*sheet))) {
      JS_ReportOutOfMemory(cx); iterator.closeThrow(); return false;
    }
  }
  {
    JSAutoRealm realm(cx, AdoptedOwnerOf(proxy).page);
    JS::RootedObject rows(cx);
    if (!AdoptedBacking(cx, proxy, &rows) || !JS::SetArrayLength(cx, rows, 0)) return false;
    for (size_t i = 0; i < converted.length(); ++i) {
      value.set(converted[i]);
      JS::ObjectOpResult result;
      if (!AdoptedPut(cx, proxy, uint32_t(i), value, result)) return false;
      if (!result) return result.reportError(cx, proxy);
    }
  }
  args.rval().setUndefined(); return true;
}
}
void Realm::SetAdoptedPorts(document_model::StyleDocumentResolvePort resolve,
    document_model::StyleScopeDocumentPort document, document_model::StyleCascadeScopePort scope, void* context) {
  state_->adoptedResolve = resolve; state_->scopeDocument = document;
  state_->cascadeScope = scope; state_->adoptedContext = context;
}
bool Realm::NewAdoptedList(JS::HandleObject scope, JS::MutableHandleValue out) {
  State& state = *state_;
  JS::RootedObject source(state.cx, scope), proxy(state.cx);
  JS::RootedValue result(state.cx);
  {
    JSAutoRealm realm(state.cx, state.page);
    if (!JS_WrapObject(state.cx, &source) || !NewAdoptedArray(state, source, &proxy)) return false;
    result.setObject(*proxy);
  }
  if (!JS_WrapValue(state.cx, &result)) return false;
  out.set(result); return true;
}
bool InstallAdoptedStyles(State& state) {
  return detail::AccessorOn(state.kit, state.documentPrototype, "Document", "adoptedStyleSheets", Get, Set, 0) &&
         detail::AccessorOn(state.kit, state.shadowPrototype, "ShadowRoot", "adoptedStyleSheets", Get, Set, 1);
}
}
