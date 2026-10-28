// History session history (T9). The BrowserEnvironmentAggregate owns the
// bounded entry list, the current index and the opaque structured-clone state
// bytes; this adapter resolves and origin-checks the pushState/replaceState
// URL, runs the engine's structured-clone round trip and delivers a trusted
// popstate through the Interaction owner's one dispatch tree. No state lives
// on the JS object or in a second history container.
#include "runtime/web_kit.h"
#include "browser_environment/history.h"
#include "browser_environment/navigation_api.h"
#include "browser_environment/state.h"
#include "browser_environment/window_members.h"
#include "runtime/page_helpers.h"
#include "domain/browser_environment/url_value.h"
#include "runtime/runtime.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/StructuredClone.h"
#include "js/String.h"
#include <string>
#include <vector>

namespace ruyijs::browser_environment {
using UrlValue = ruyijs::domain::browser_environment::UrlValue;
namespace {
State* Receiver(JSContext* cx, const JS::CallArgs& args, int32_t* slot) {
  auto* state = static_cast<State*>(
      js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  *slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  State* receiver = state ? ReceiverState(args, ClassFor(Kind::History)) : nullptr;
  if (!receiver) {
    WebKit::FailBrand(cx, args, "History");
    return nullptr;
  }
  state = receiver;
  return state;
}
// Install-time names of the History methods, so the debug monitor names the
// member the installer registered.
const char* MethodName(int32_t slot) {
  switch (slot) {
    case kHistoryPushState: return "pushState";
    case kHistoryReplaceState: return "replaceState";
    case kHistoryGo: return "go";
    case kHistoryBack: return "back";
    case kHistoryForward: return "forward";
    default: return "";
  }
}
}  // namespace
// StructuredSerialize: the engine's own same-process serializer, copied out as
// opaque bytes. Any failure is the spec's DataCloneError and happens before the
// domain entry is touched, so a bad state never partially commits.
bool EncodeState(State& state, JS::HandleValue value, std::vector<uint8_t>* bytes) {
  JSContext* cx = state.cx;
  JSAutoStructuredCloneBuffer buffer(JS::StructuredCloneScope::SameProcess, nullptr, nullptr);
  if (!buffer.write(cx, value, nullptr, nullptr)) {
    JS_ClearPendingException(cx);
    return ThrowDOM(state, "DataCloneError", "The object could not be cloned");
  }
  buffer.data().ForEachDataChunk([&](const char* data, size_t size) {
    bytes->insert(bytes->end(), reinterpret_cast<const uint8_t*>(data),
                  reinterpret_cast<const uint8_t*>(data) + size);
    return true;
  });
  return true;
}
// StructuredDeserialize of the current entry's bytes. An empty blob is the
// initial document entry's unset state and reads back as null.
bool DecodeState(JSContext* cx, const std::vector<uint8_t>& bytes,
                 JS::MutableHandleValue result) {
  if (bytes.empty()) { result.setNull(); return true; }
  JSStructuredCloneData data(JS::StructuredCloneScope::SameProcess);
  if (!data.Init(bytes.size()) ||
      !data.AppendBytes(reinterpret_cast<const char*>(bytes.data()), bytes.size())) {
    JS_ReportOutOfMemory(cx);
    return false;
  }
  JS::CloneDataPolicy policy;
  if (!JS_ReadStructuredClone(cx, data, JS_STRUCTURED_CLONE_VERSION,
                              JS::StructuredCloneScope::SameProcess, result, policy,
                              nullptr, nullptr)) {
    JS_ClearPendingException(cx);
    return Error(cx, "History state could not be deserialized");
  }
  return true;
}
namespace {
// The pushState/replaceState URL: resolved against the current location and
// required to stay same-origin. A parse failure and a cross-origin result are
// both the spec's SecurityError. An absent url keeps the current location.
bool ResolveURL(State& state, const std::string& input, bool has_url, std::string* out) {
  JSContext* cx = state.cx;
  JS::RootedValue current(cx);
  if (!state.Read(Kind::Location, 0, ValueType::String, &current)) return false;
  JS::RootedString text(cx, current.toString());
  std::string base;
  if (!text || !ToUTF8(cx, text, &base)) return false;
  if (!has_url) { *out = base; return true; }
  UrlValue resolved;
  if (!UrlValue::Resolve(input, base, &resolved))
    return ThrowDOM(state, "SecurityError",
                    "The operation is insecure: the URL could not be resolved");
  UrlValue base_url;
  if (!UrlValue::Resolve(base, "", &base_url) || base_url.origin() != resolved.origin())
    return ThrowDOM(state, "SecurityError",
                    "The operation is insecure: a history entry may not change the origin");
  *out = resolved.href();
  return true;
}
bool PushOrReplace(State& state, const JS::CallArgs& args, bool replace) {
  JSContext* cx = state.cx;
  // WebIDL argument conversion happens before the operation body.
  if (args.length() < 2)
    return Error(cx, "History state operations require two arguments");
  JS::RootedString title(cx, JS::ToString(cx, args.get(1)));
  if (!title) return false;
  bool has_url = !args.get(2).isUndefined() && !args.get(2).isNull();
  std::string input;
  if (has_url) {
    JS::RootedString url_text(cx, JS::ToString(cx, args.get(2)));
    if (!url_text || !ToUTF8(cx, url_text, &input)) return false;
  }
  std::vector<uint8_t> bytes;
  if (!EncodeState(state, args.get(0), &bytes)) return false;
  std::string url;
  if (!ResolveURL(state, input, has_url, &url)) return false;
  // Even when a child commit is unsupported, failed inputs follow the same
  // serialization and URL checks as the browser before reaching that boundary.
  if (state.shared) return Error(cx, "iframe navigation is not implemented");
  // The Navigation API's navigate event may cancel the commit; the
  // currententrychange event follows it (docs/owners/BROWSER_ENVIRONMENT.md).
  const char* type = replace ? "replace" : "push";
  JS::RootedValue from(cx), none(cx);
  bool proceed = true;
  args.rval().setUndefined();
  if (!NavigationBeforeCommit(state, type, url, false, none, none, &from, &proceed)) return false;
  if (!proceed) return true;
  bool ok = replace ? state.model.HistoryReplace(url, bytes) : state.model.HistoryPush(url, bytes);
  if (!ok) { JS_ReportOutOfMemory(cx); return false; }
  return NavigationAfterCommit(state, type, from);
}
bool Go(State& state, const JS::CallArgs& args, int32_t fixed) {
  JSContext* cx = state.cx;
  if (state.shared) return Error(cx, "iframe navigation is not implemented");
  int32_t delta = fixed;
  if (fixed == 0 && !JS::ToInt32(cx, args.get(0), &delta)) return false;
  // A same-entry traversal (go(0)) is the spec's reload, which this runtime
  // does not perform; an out-of-range traversal is a no-op and queues nothing.
  if (delta == 0) { args.rval().setUndefined(); return true; }
  int32_t index = 0;
  JS::RootedValue length(cx);
  if (!state.model.HistoryIndex(&index) ||
      !state.Read(Kind::History, 0, ValueType::Integer, &length)) {
    JS_ReportOutOfMemory(cx);
    return false;
  }
  long long target = static_cast<long long>(index) + delta;
  if (target < 0 || target >= static_cast<long long>(length.toInt32())) {
    args.rval().setUndefined();
    return true;
  }
  if (!state.postTask)
    return Error(cx, "history traversal requires the scheduling owner");
  JS::RootedFunction deliver(cx,
      js::NewFunctionWithReserved(cx, DeliverPopState, 0, 0, "popstate"));
  if (!deliver) return false;
  JS::RootedObject trampoline(cx, JS_GetFunctionObject(deliver));
  js::SetFunctionNativeReserved(trampoline, 0, JS::PrivateValue(&state));
  js::SetFunctionNativeReserved(trampoline, 1, JS::Int32Value(delta));
  if (!state.postTask(state.postTaskContext, trampoline)) return false;
  args.rval().setUndefined();
  return true;
}
}  // namespace
// The traversal task: commit the index, update the current entry's URL and fire
// one trusted popstate with the restored state. Queued on the one Scheduling
// task queue through the same postTask port Window.postMessage uses.
bool DeliverPopState(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  State* state = static_cast<State*>(
      js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  if (!state || !state->deliverPopState)
    return Error(cx, "popstate delivery is not bound");
  int32_t delta = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  bool changed = false;
  if (!state->model.HistoryGo(delta, &changed) || !changed) return true;
  std::vector<uint8_t> bytes;
  if (!state->model.HistoryState(&bytes)) { JS_ReportOutOfMemory(cx); return false; }
  JS::RootedValue value(cx, JS::NullValue());
  if (!bytes.empty() && !DecodeState(cx, bytes, &value)) return false;
  return state->deliverPopState(state->deliverPopStateContext, state->page, value);
}
bool HistoryMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int32_t slot = 0;
  State* state = Receiver(cx, args, &slot);
  if (!state) return false;
  state->Observe("History", MethodName(slot), int(ApiAccess::Call));
  switch (slot) {
    case kHistoryPushState: return PushOrReplace(*state, args, false);
    case kHistoryReplaceState: return PushOrReplace(*state, args, true);
    case kHistoryGo: return Go(*state, args, 0);
    case kHistoryBack: return Go(*state, args, -1);
    case kHistoryForward: return Go(*state, args, 1);
    default: return Error(cx, "History member is not installed");
  }
}
bool HistoryStateGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int32_t slot = 0;
  State* state = Receiver(cx, args, &slot);
  if (!state) return false;
  if (slot != kHistoryState) return Error(cx, "History member is not installed");
  state->Observe("History", "state", int(ApiAccess::Read));
  Model& model = state->shared ? state->shared->model : state->model;
  std::vector<uint8_t> bytes;
  if (!model.HistoryState(&bytes)) { JS_ReportOutOfMemory(cx); return false; }
  return DecodeState(cx, bytes, args.rval());
}
// scrollRestoration is the one History member that is not an operation: it
// reads and writes the session-history owner's flag (Firefox default "auto").
bool HistoryScrollRestorationGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int32_t slot = 0;
  State* state = Receiver(cx, args, &slot);
  if (!state) return false;
  if (slot != kHistoryScrollRestoration)
    return Error(cx, "History member is not installed");
  state->Observe("History", "scrollRestoration", int(ApiAccess::Read));
  Model& model = state->shared ? state->shared->model : state->model;
  bool manual = false;
  if (!model.ScrollRestoration(&manual)) { JS_ReportOutOfMemory(cx); return false; }
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, manual ? "manual" : "auto"));
  if (!text) return false;
  args.rval().setString(text);
  return true;
}
bool HistoryScrollRestorationSetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int32_t slot = 0;
  State* state = Receiver(cx, args, &slot);
  if (!state) return false;
  if (slot != kHistoryScrollRestoration)
    return Error(cx, "History member is not installed");
  state->Observe("History", "scrollRestoration", int(ApiAccess::Write));
  JS::RootedString input(cx, JS::ToString(cx, args.get(0)));
  std::string value;
  if (!input || !ToUTF8(cx, input, &value)) return false;
  // The IDL is an enum DOMString: any other value is a TypeError, as in
  // Firefox.
  if (value != "auto" && value != "manual")
    return Error(cx, "scrollRestoration must be \"auto\" or \"manual\"");
  Model& model = state->shared ? state->shared->model : state->model;
  if (!model.SetScrollRestoration(value == "manual")) {
    JS_ReportOutOfMemory(cx);
    return false;
  }
  args.rval().setUndefined();
  return true;
}
bool InstallHistoryMembers(State& state, JS::HandleObject prototype) {
  struct MethodSpec { const char* name; int32_t slot; unsigned length; };
  static const MethodSpec kMethods[] = {
    {"pushState", kHistoryPushState, 2}, {"replaceState", kHistoryReplaceState, 2},
    {"go", kHistoryGo, 1}, {"back", kHistoryBack, 0}, {"forward", kHistoryForward, 0},
  };
  for (const auto& spec : kMethods) {
    JS::RootedObject method(state.cx,
        state.Function(HistoryMethod, spec.name, spec.length, spec.slot));
    if (!method || !JS_DefineProperty(state.cx, prototype, spec.name, method, 0) ||
        !state.helpers->Member(method, "History", spec.name, "call")) return false;
  }
  if (!state.Accessor(prototype, "state", kHistoryState, JSPROP_ENUMERATE, HistoryStateGetter) ||
      !state.helpers->Accessor("History", prototype, "state")) return false;
  if (!state.Accessor(prototype, "scrollRestoration", kHistoryScrollRestoration,
                      JSPROP_ENUMERATE, HistoryScrollRestorationGetter,
                      HistoryScrollRestorationSetter) ||
      !state.helpers->Accessor("History", prototype, "scrollRestoration")) return false;
  return true;
}
}  // namespace ruyijs::browser_environment
