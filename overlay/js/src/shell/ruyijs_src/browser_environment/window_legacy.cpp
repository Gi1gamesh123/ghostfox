// Firefox Window legacy members (docs/owners/BROWSER_ENVIRONMENT.md, 2026-10-21).
// Each member reads the owner that already holds its state: scroll position and
// viewport here, the current event in Interaction, the document scroll extent
// in Geometry through the scrollExtent port. Replaceable attributes shadow with
// an own data property, as Firefox's [Replaceable] setters do.
#include "browser_environment/window_legacy.h"
#include "browser_environment/window_members.h"
#include "interaction/realm.h"
#include "runtime/page_helpers.h"
#include "runtime/runtime.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include <algorithm>
#include <cmath>

namespace ruyijs::browser_environment {
WindowLegacyState::WindowLegacyState(State& e, interaction::Realm& ev)
    : env(e), events(ev), kit(e.cx, e.page, *e.helpers, this), pip(e.cx), pipInstance(e.cx), onenter(e.cx) {}
namespace {
enum Member {
  mDefaultStyle_unused, mScrollByLines, mScrollByPages, mUpdateCommands, mFind, mDump, mSetResizable,
  mStatus, mEvent, mClientInformation, mScrollMaxX, mScrollMaxY, mFullScreen, mInstallTrigger,
  mOriginAgentCluster, mPictureInPicture
};
const char* const kNames[] = {"getDefaultComputedStyle", "scrollByLines", "scrollByPages", "updateCommands", "find",
                              "dump", "setResizable", "status", "event", "clientInformation", "scrollMaxX",
                              "scrollMaxY", "fullScreen", "InstallTrigger", "originAgentCluster",
                              "documentPictureInPicture"};
WindowLegacyState& Self(const JS::CallArgs& args) { return *static_cast<WindowLegacyState*>(WebKit::Owner(args)); }
bool Required(JSContext* cx, const JS::CallArgs& args, unsigned count, int member) {
  if (args.length() >= count) return true;
  return WebKit::Fail(cx, (std::string("Window.") + kNames[member] + ": At least " + std::to_string(count) +
                           " argument required, but only " + std::to_string(args.length()) + " passed").c_str());
}
// ScrollOptions.behavior: the ScrollBehavior enumeration.
bool ReadScrollOptions(JSContext* cx, JS::HandleValue value, int member) {
  if (value.isNullOrUndefined()) return true;
  if (!value.isObject())
    return WebKit::Fail(cx, (std::string("Window.") + kNames[member] + ": Argument 2 can't be converted to a dictionary.").c_str());
  JS::RootedObject options(cx, &value.toObject());
  JS::RootedValue behavior(cx);
  if (!JS_GetProperty(cx, options, "behavior", &behavior)) return false;
  if (behavior.isUndefined()) return true;
  JS::RootedString text(cx, JS::ToString(cx, behavior));
  std::string name;
  if (!text || !ToUTF8(cx, text, &name)) return false;
  if (name == "auto" || name == "instant" || name == "smooth") return true;
  return WebKit::Fail(cx, (std::string("Window.") + kNames[member] + ": '" + name + "' (value of 'behavior' member of "
                           "ScrollOptions) is not a valid value for enumeration ScrollBehavior.").c_str());
}
// The deferred scroll step: Firefox applies line/page scrolling after the call
// returns (scroll-frame mode), so the position changes in a later task.
bool ScrollTask(JSContext*, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  const double delta = js::GetFunctionNativeReserved(&args.callee(), 1).toNumber();
  args.rval().setUndefined();
  return !std::isfinite(state.scrollY + delta) || ScrollWindowTo(state, state.scrollX, state.scrollY + delta);
}
bool ScrollBy(WindowLegacyState& w, const JS::CallArgs& args, int member) {
  JSContext* cx = w.env.cx;
  int32_t count = 0;
  if (!Required(cx, args, 1, member) || !JS::ToInt32(cx, args[0], &count) ||
      !ReadScrollOptions(cx, args.get(1), member)) return false;
  args.rval().setUndefined();
  if (!count || !w.env.postTask) return true;
  double width = 0, height = 0;
  if (!w.env.ReadViewport(&width, &height)) return false;
  // Line: the default font's line box; page: the scrollport less Firefox's 10% overlap.
  const double step = member == mScrollByLines ? 19.0 : std::floor(height * 0.9);
  JSFunction* task = js::NewFunctionWithReserved(cx, ScrollTask, 0, 0, kNames[member]);
  if (!task) return false;
  JS::RootedObject object(cx, JS_GetFunctionObject(task));
  js::SetFunctionNativeReserved(object, 0, JS::PrivateValue(&w.env));
  js::SetFunctionNativeReserved(object, 1, JS::DoubleValue(step * count));
  return w.env.postTask(w.env.postTaskContext, object);
}
bool ScrollMax(WindowLegacyState& w, bool vertical, JS::MutableHandleValue out) {
  double width = 0, height = 0, right = 0, bottom = 0;
  if (!w.env.ReadViewport(&width, &height)) return false;
  if (w.env.scrollExtent && !w.env.scrollExtent(w.env.scrollExtentContext, &right, &bottom)) return false;
  const double extent = vertical ? bottom + w.env.scrollY - height : right + w.env.scrollX - width;
  out.setInt32(int32_t(std::max(0.0, std::round(std::isfinite(extent) ? extent : 0))));
  return true;
}
bool LegacyMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  WindowLegacyState& w = Self(args);
  const int member = WebKit::Slot(args);
  w.env.Observe("Window", kNames[member], int(ApiAccess::Call));
  JS::RootedString text(cx);
  switch (member) {
    case mScrollByLines:
    case mScrollByPages:
      return ScrollBy(w, args, member);
    case mUpdateCommands:  // chrome-only command updates: none in a content window
    case mDump:            // browser.dom.window.dump.enabled is false in content
      if (!Required(cx, args, 1, member)) return false;
      text = JS::ToString(cx, args[0]);
      if (!text) return false;
      args.rval().setUndefined();
      return true;
    case mSetResizable:  // a no-op for content windows in Firefox
      if (!Required(cx, args, 1, member)) return false;
      (void)JS::ToBoolean(args[0]);
      args.rval().setUndefined();
      return true;
    default: {  // find(string, caseSensitive, backwards, wrapAround, wholeWord, searchInFrames, showDialog)
      if (argc && !args[0].isUndefined() && !(text = JS::ToString(cx, args[0]))) return false;
      args.rval().setBoolean(false);
      return true;
    }
  }
}
bool LegacyGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  WindowLegacyState& w = Self(args);
  const int member = WebKit::Slot(args);
  w.env.Observe("Window", kNames[member], int(ApiAccess::Read));
  switch (member) {
    case mStatus: {
      JSString* s = JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(w.status.data(), w.status.size()));
      if (!s) return false;
      args.rval().setString(s);
      return true;
    }
    case mEvent: return w.events.CurrentEvent(args.rval());
    case mClientInformation:
      args.rval().setObject(*w.env.instances.get()[int32_t(Kind::Navigator)]);
      return JS_WrapValue(cx, args.rval());
    case mScrollMaxX: return ScrollMax(w, false, args.rval());
    case mScrollMaxY: return ScrollMax(w, true, args.rval());
    case mFullScreen: case mOriginAgentCluster: args.rval().setBoolean(false); return true;
    case mInstallTrigger: args.rval().setNull(); return true;
    default:
      args.rval().setObject(*w.pipInstance);
      return JS_WrapValue(cx, args.rval());
  }
}
bool LegacySetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  WindowLegacyState& w = Self(args);
  const int member = WebKit::Slot(args);
  w.env.Observe("Window", kNames[member], int(ApiAccess::Write));
  args.rval().setUndefined();
  if (member == mStatus) {
    JS::RootedString text(cx, JS::ToString(cx, args.get(0)));
    std::string value;
    if (!text || !ToUTF8(cx, text, &value)) return false;
    w.status = std::move(value);
    return true;
  }
  if (member == mFullScreen) {  // only chrome may change the fullscreen state
    (void)JS::ToBoolean(args.get(0));
    return true;
  }
  JS::RootedObject page(cx, w.env.page);  // [Replaceable]
  return JS_DefineProperty(cx, page, kNames[member], args.get(0), JSPROP_ENUMERATE);
}
}  // namespace

bool InstallWindowLegacy(State& state, interaction::Realm& events) {
  if (state.worker) return true;
  JSContext* cx = state.cx;
  state.legacy = std::make_shared<WindowLegacyState>(state, events);
  WindowLegacyState& w = *state.legacy;
  JS::RootedObject page(cx, state.page);
  const unsigned lengths[] = {1, 1, 1, 1, 0, 1, 1};
  for (int m = mScrollByLines; m <= mSetResizable; ++m) {
    JS::RootedObject method(cx, w.kit.Function(LegacyMethod, kNames[m], lengths[m], m));
    if (!method || !JS_DefineProperty(cx, page, kNames[m], method, JSPROP_ENUMERATE) ||
        !state.helpers->Member(method, "Window", kNames[m], "call")) return false;
  }
  if (!InstallDocumentPictureInPicture(w) || !InstallPictureInPictureWindow(w)) return false;
  for (int m = mStatus; m <= mPictureInPicture; ++m) {
    const bool readonly = m == mOriginAgentCluster || m == mPictureInPicture;
    JS::RootedObject get(cx, w.kit.Function(LegacyGetter, (std::string("get ") + kNames[m]).c_str(), 0, m));
    JS::RootedObject set(cx, readonly ? nullptr : w.kit.Function(LegacySetter, (std::string("set ") + kNames[m]).c_str(), 1, m));
    if (!get || (!readonly && !set) || !JS_DefineProperty(cx, page, kNames[m], get, set, JSPROP_ENUMERATE) ||
        !state.helpers->Accessor("Window", page, kNames[m])) return false;
  }
  // window.netscape.security: two empty plain objects, defined writable,
  // configurable and non-enumerable as Firefox's InitializeLegacyNetscapeObject
  // does (JS_DefineObject with no attributes).
  JS::RootedObject netscape(cx, JS_DefineObject(cx, page, "netscape", nullptr, 0));
  return netscape && JS_DefineObject(cx, netscape, "security", nullptr, 0) &&
         state.helpers->Object("netscape", netscape);
}
}  // namespace ruyijs::browser_environment
