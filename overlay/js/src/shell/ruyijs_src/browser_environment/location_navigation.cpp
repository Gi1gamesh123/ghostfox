// Location setter family and navigation (T10). Location is Firefox's one
// unforgeable URL interface: href and the six component members are writable,
// origin is not, and assign/replace/reload plus toString live on the instance.
//
// Two behaviours are deliberately distinct here. A full navigation hands the
// resolved URL to the runtime's single PageLoad coordinator through the
// navigation port and the document that coordinator loads replaces this one. A
// fragment-only change is a same-document navigation: it commits one history
// entry through the same session-history owner and delivers hashchange, and the
// document is never reloaded.
#include "browser_environment/location_navigation.h"
#include "browser_environment/state.h"
#include "browser_environment/window_members.h"
#include "runtime/page_helpers.h"
#include "runtime/runtime.h"
#include "domain/browser_environment/url_value.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include <string>
#include <vector>

namespace ruyijs::browser_environment {
using UrlValue = ruyijs::domain::browser_environment::UrlValue;
namespace {
constexpr uint32_t kLocationHref = 0;
constexpr uint32_t kLocationHash = 7;
// Install-time names of the Location navigation methods, so the debug monitor
// names the member the installer registered.
const char* MethodName(int32_t slot) {
  switch (slot) {
    case kLocationAssign: return "assign";
    case kLocationReplace: return "replace";
    case kLocationReload: return "reload";
    default: return "";
  }
}

State* Receiver(JSContext* cx, const JS::CallArgs& args, int32_t* slot) {
  auto* state = static_cast<State*>(
      js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  *slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  State* receiver = state ? ReceiverState(args, ClassFor(Kind::Location)) : nullptr;
  if (!receiver) {
    Error(cx, "Location member called on incompatible receiver");
    return nullptr;
  }
  state = receiver;
  return state;
}

bool Href(State& state, std::string* href) {
  JS::RootedValue value(state.cx);
  if (!state.Read(Kind::Location, kLocationHref, ValueType::String, &value)) return false;
  JS::RootedString text(state.cx, value.toString());
  return ToUTF8(state.cx, text, href);
}

// The one gate every JS-driven navigation passes: the runtime coordinator owns
// the document load, so a missing port is a programming error, not a page
// condition.
bool Navigate(State& state, const std::string& href, bool replace, bool reload = false) {
  if (!state.navigate) return Error(state.cx, "navigation requires the runtime coordinator");
  return state.navigate(state.navigateContext, href.c_str(), replace, reload);
}

// A same-document fragment change: one session-history entry through the same
// owner the History interface uses, then one trusted hashchange. An unchanged
// URL commits nothing and fires nothing, as in Firefox.
bool SetFragment(State& state, const std::string& href, bool replace) {
  std::string previous;
  if (!Href(state, &previous)) return false;
  if (href == previous) return true;
  // A child's URL belongs to its frame, not the parent's shared History model.
  // Full iframe history is separate debt; hash changes must never navigate
  // the embedding page or write one of its history entries.
  if (state.shared) {
    state.frameUrl = href;
    return DeliverHashChange(state, previous.c_str(), href.c_str());
  }
  // A replacing fragment navigation keeps the entry's structured-clone
  // state; a pushing one starts the new entry with unset (null) state.
  std::vector<uint8_t> state_bytes;
  if (replace && !state.model.HistoryState(&state_bytes)) {
    JS_ReportOutOfMemory(state.cx);
    return false;
  }
  bool ok = replace ? state.model.HistoryReplace(href, state_bytes)
                    : state.model.HistoryPush(href, {});
  if (!ok) {
    JS_ReportOutOfMemory(state.cx);
    return false;
  }
  return DeliverHashChange(state, previous.c_str(), href.c_str());
}

// Firefox treats a target that differs from the current URL only in its
// fragment as a same-document navigation instead of a reload, so href,
// assign and replace all have to distinguish it from a real navigation.
bool FragmentOnly(const std::string& current, const std::string& target) {
  if (current == target) return false;
  size_t current_hash = current.find('#');
  size_t target_hash = target.find('#');
  return current.substr(0, current_hash) == target.substr(0, target_hash);
}

bool Resolve(State& state, const std::string& input, const std::string& base,
             std::string* href) {
  UrlValue resolved;
  if (!UrlValue::Resolve(input, base, &resolved))
    return ThrowDOM(state, "SyntaxError",
                    "The string did not match the expected pattern");
  *href = resolved.href();
  return true;
}
}  // namespace

bool DeliverHashChange(State& state, const char* oldURL, const char* newURL) {
  if (!state.deliverHashChange)
    return Error(state.cx, "hashchange delivery requires the interaction owner");
  return state.deliverHashChange(state.deliverHashChangeContext, state.page, oldURL, newURL);
}

bool LocationSetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int32_t slot = 0;
  State* state = Receiver(cx, args, &slot);
  if (!state) return false;
  uint32_t index = uint32_t(slot % 32);
  state->Observe("Location", MemberName(Kind::Location, index), int(ApiAccess::Write));
  JS::RootedString text(cx, JS::ToString(cx, args.get(0)));
  if (!text) return false;
  std::string input, current;
  if (!ToUTF8(cx, text, &input) || !Href(*state, &current)) return false;
  if (index == kLocationHash) {
    UrlValue url(current);
    url.set_hash(input);
    return SetFragment(*state, url.href(), false);
  }
  std::string href;
  if (index == kLocationHref) {
    if (!Resolve(*state, input, current, &href)) return false;
  } else {
    UrlValue url(current);
    switch (index) {
      case 1: url.set_protocol(input); break;
      case 2: url.set_host(input); break;
      case 3: url.set_hostname(input); break;
      case 4: url.set_port(input); break;
      case 5: url.set_pathname(input); break;
      case 6: url.set_search(input); break;
      default: return Error(cx, "Location member is not installed");
    }
    href = url.href();
    // An unusable or unchanged component leaves the URL alone, which is the
    // URL standard's "ignore" outcome rather than a navigation.
    if (href == current) { args.rval().setUndefined(); return true; }
  }
  if (FragmentOnly(current, href)) return SetFragment(*state, href, false);
  if (!Navigate(*state, href, false)) return false;
  args.rval().setUndefined();
  return true;
}

bool LocationMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int32_t slot = 0;
  State* state = Receiver(cx, args, &slot);
  if (!state) return false;
  state->Observe("Location", MethodName(slot), int(ApiAccess::Call));
  std::string current;
  if (!Href(*state, &current)) return false;
  if (slot == kLocationReload) {
    if (!Navigate(*state, current, true, true)) return false;
    args.rval().setUndefined();
    return true;
  }
  if (slot != kLocationAssign && slot != kLocationReplace)
    return Error(cx, "Location member is not installed");
  JS::RootedString text(cx, JS::ToString(cx, args.get(0)));
  if (!text) return false;
  std::string input, href;
  if (!ToUTF8(cx, text, &input) || !Resolve(*state, input, current, &href)) return false;
  bool replace = slot == kLocationReplace;
  if (FragmentOnly(current, href)) return SetFragment(*state, href, replace);
  if (!Navigate(*state, href, replace)) return false;
  args.rval().setUndefined();
  return true;
}

bool InstallLocationMembers(State& state, JS::HandleObject location) {
  struct MethodSpec { const char* name; int32_t slot; unsigned length; };
  static const MethodSpec kMethods[] = {
    {"assign", kLocationAssign, 1}, {"replace", kLocationReplace, 1},
    {"reload", kLocationReload, 0},
  };
  for (const auto& spec : kMethods) {
    JS::RootedObject method(state.cx,
        state.Function(LocationMethod, spec.name, spec.length, spec.slot));
    if (!method || !JS_DefineProperty(state.cx, location, spec.name, method,
                        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY) ||
        !state.helpers->Member(method, "Location", spec.name, "call")) return false;
  }
  return true;
}
}  // namespace ruyijs::browser_environment
