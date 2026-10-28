// Window facade methods. Every ruling for the headless runtime is explicit:
// alert performs no I/O and only records the invocation (observable through the
// Window member trace), close() marks this window closed without tearing the
// Context down, open() returns null because no new browsing context is created,
// Selection is owned by Document Model. None return the silent undefined that an
// uninstalled member would. T5b's second slice installs the rest of the donor's
// operations: confirm()/prompt() report the disabled-dialog result, print/stop/
// focus/blur/captureEvents/releaseEvents are no-ops, moveTo/moveBy/resizeTo/
// resizeBy convert their WebIDL long arguments and then silently do nothing
// (donor CanMoveResizeWindows is false for this runtime), and reportError fires
// one trusted error event on the window. The page's print is a DOM method; the
// embedded shell keeps its own host control in the shell global, which is a
// different global from the ruyijsPage global (runtime/evaluation.cpp).
#include "runtime/web_kit.h"
#include "browser_environment/state.h"
#include "browser_environment/window_members.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include <cmath>

namespace ruyijs::browser_environment {
namespace {
State* Receiver(JSContext* cx, const JS::CallArgs& args, int32_t* slot) {
  auto* state = static_cast<State*>(
      js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  *slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  // A bare call through the global object environment record hands the native an
  // undefined this value (SpiderMonkey does not coerce it for JSNatives), and an
  // explicit call/apply can pass null; the donor's MaybeGlobalThisPolicy accepts
  // both and resolves them to the callee realm's global
  // (dom/bindings/BindingUtils.cpp:2863-2874), which this window is. Any other
  // non-matching receiver is still rejected.
  bool is_page = args.thisv().isNullOrUndefined() ||
                 (args.thisv().isObject() && &args.thisv().toObject() == state->page);
  if (!state || !is_page) {
    WebKit::FailBrand(cx, args, "Window");
    return nullptr;
  }
  return state;
}
// options[name] with an alternate key, resolved only when it is present.
bool Option(JSContext* cx, JS::HandleObject options, const char* primary,
            const char* alternate, JS::MutableHandleValue result) {
  result.setUndefined();
  bool present = false;
  if (!JS_HasProperty(cx, options, primary, &present)) return false;
  const char* name = primary;
  if (!present) {
    if (!JS_HasProperty(cx, options, alternate, &present)) return false;
    name = alternate;
  }
  return !present || JS_GetProperty(cx, options, name, result);
}
// ScrollToOptions/coordinates: reads one axis and reports whether it was given.
bool Axis(State& state, const JS::CallArgs& args, const char* primary,
          const char* alternate, uint32_t index, bool* present, double* value) {
  JS::RootedValue raw(state.cx);
  *present = false;
  if (args.length() >= 1 && args[0].isObject()) {
    JS::RootedObject options(state.cx, &args[0].toObject());
    if (!Option(state.cx, options, primary, alternate, &raw)) return false;
  } else if (args.length() > index) {
    raw.set(args[index]);
  } else {
    return true;
  }
  if (raw.isUndefined()) return true;
  *present = true;
  return JS::ToNumber(state.cx, raw, value);
}
bool Scroll(State& state, const JS::CallArgs& args, bool relative) {
  double x = state.scrollX, y = state.scrollY;
  bool has_x = false, has_y = false, value = false;
  value = Axis(state, args, "left", "x", 0, &has_x, &x);
  if (!value) return false;
  value = Axis(state, args, "top", "y", 1, &has_y, &y);
  if (!value) return false;
  if (relative) {
    if (has_x) x += state.scrollX;
    else x = state.scrollX;
    if (has_y) y += state.scrollY;
    else y = state.scrollY;
  } else {
    if (!has_x) x = state.scrollX;
    if (!has_y) y = state.scrollY;
  }
  return ScrollWindowTo(state, std::isfinite(x) ? x : state.scrollX, std::isfinite(y) ? y : state.scrollY);
}
}
bool WindowMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int32_t slot = 0;
  State* state = Receiver(cx, args, &slot);
  if (!state) return false;
  state->Observe("Window", WindowMemberName(slot), int(ApiAccess::Call));
  switch (slot) {
    case kWindowAlert: {
      // The message is still converted, so a throwing toString() keeps its
      // spec-visible effect even though nothing is displayed.
      JS::RootedString message(cx, JS::ToString(cx, args.get(0)));
      if (!message) return false;
      ++state->alertCount;
      break;
    }
    case kWindowClose:
      state->closed = true;
      break;
    case kWindowOpen: {
      JS::RootedString url(cx, JS::ToString(cx, args.get(0)));
      if (!url) return false;
      args.rval().setNull();
      return true;
    }
    case kWindowScrollTo:
    case kWindowScrollBy:
    case kWindowScroll:
      if (!Scroll(*state, args, slot == kWindowScrollBy)) return false;
      break;
    case kWindowConfirm: {
      // The optional message is still converted (ToString), then the disabled
      // dialog reports false without prompting (donor ConfirmOuter).
      JS::RootedString message(cx, JS::ToString(cx, args.get(0)));
      if (!message) return false;
      args.rval().setBoolean(false);
      return true;
    }
    case kWindowPrompt: {
      JS::RootedString message(cx, JS::ToString(cx, args.get(0)));
      JS::RootedString initial(cx, JS::ToString(cx, args.get(1)));
      if (!message || !initial) return false;
      args.rval().setNull();
      return true;
    }
    case kWindowPrint:
    case kWindowStop:
    case kWindowFocus:
    case kWindowBlur:
    case kWindowCaptureEvents:
    case kWindowReleaseEvents:
      break;
    case kWindowMoveTo:
    case kWindowMoveBy:
    case kWindowResizeTo:
    case kWindowResizeBy: {
      // WebIDL long conversion happens before the donor silently ignores a
      // request it is not allowed to honour.
      int32_t first = 0, second = 0;
      if (!JS::ToInt32(cx, args.get(0), &first) ||
          !JS::ToInt32(cx, args.get(1), &second)) return false;
      break;
    }
    case kWindowReportError: {
      if (args.length() < 1)
        return Error(cx, "reportError requires at least 1 argument");
      if (state->fireEvent &&
          !state->fireEvent(state->fireEventContext, state->page, "error"))
        return false;
      break;
    }
    default:
      return Error(cx, "Window method is not installed");
  }
  args.rval().setUndefined();
  return true;
}
}
