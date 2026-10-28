// Window facade scalars. Content viewport dimensions read the original
// environment owner, whose child-context port measures the frame content box.
#include "runtime/web_kit.h"
#include "browser_environment/state.h"
#include "browser_environment/window_members.h"
#include "browser_environment/browser_chrome.h"
#include "runtime/page_helpers.h"
#include "runtime/runtime.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "browser_environment/window_clone.h"
#include "js/String.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

namespace ruyijs::browser_environment {
namespace {
State* Receiver(JSContext* cx, const JS::CallArgs& args, int32_t* slot) {
  auto* state = static_cast<State*>(
      js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  *slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  if (*slot == kWindowInnerWidth || *slot == kWindowInnerHeight) {
    JS::RootedObject prototype(cx);
    return WindowOwner(cx, args, &prototype);
  }
  // A bare call through the global object environment record hands the native an
  // undefined this value (SpiderMonkey does not coerce it for JSNatives), and an
  // explicit call/apply can pass null; the donor's MaybeGlobalThisPolicy accepts
  // both and resolves them to the callee realm's global
  // (dom/bindings/BindingUtils.cpp:2863-2874), which this window is. Any other
  // non-matching receiver is still rejected.
  bool is_page = state && (args.thisv().isNullOrUndefined() ||
                 (args.thisv().isObject() && &args.thisv().toObject() == state->page));
  if (!state || !is_page) {
    WebKit::FailBrand(cx, args, "Window");
    return nullptr;
  }
  return state;
}
bool ScreenField(State& state, uint32_t field, JS::MutableHandleValue result) {
  return state.Read(Kind::Screen, field, ValueType::Integer, result);
}
// The content area's screen origin (Firefox mozInnerScreenX/Y): the window
// position plus the window frame border (half the outer/inner width
// difference) and, vertically, the browser chrome above the content. Firefox
// trace 20261021-160155: screenX 22, outerWidth 1296, innerWidth 1280 ->
// mozInnerScreenX 30. Uses the top-level viewport, so an iframe reports its
// top window's origin (an offset frame is a recorded deviation, D46).
bool InnerScreen(State& state, double* x, double* y) {
  JS::RootedValue sx(state.cx), sy(state.cx), ow(state.cx), oh(state.cx), iw(state.cx), ih(state.cx);
  if (!state.Read(Kind::Screen, 4, ValueType::Integer, &sx) || !state.Read(Kind::Screen, 5, ValueType::Integer, &sy) ||
      !state.Read(Kind::Screen, 0, ValueType::Integer, &ow) || !state.Read(Kind::Screen, 1, ValueType::Integer, &oh) ||
      !state.Read(Kind::Viewport, 0, ValueType::Integer, &iw) || !state.Read(Kind::Viewport, 1, ValueType::Integer, &ih))
    return false;
  // Windows Firefox's frame border is 8px; the profile's outer size is the
  // screen size, so a smaller configured viewport must not widen the border.
  const double border = std::min(8.0, std::max(0.0, (ow.toNumber() - iw.toNumber()) / 2));
  *x = sx.toNumber() + border;
  *y = sy.toNumber() + std::max(0.0, oh.toNumber() - ih.toNumber() - border);
  return true;
}
bool InnerScreenField(State& state, bool vertical, JS::MutableHandleValue result) {
  double x = 0, y = 0;
  if (!InnerScreen(state, &x, &y)) return false;
  result.setNumber(vertical ? y : x);
  return true;
}
bool ViewportField(State& state, bool height, JS::MutableHandleValue result) {
  double width = 0, value = 0;
  if (!state.ReadViewport(&width, &value)) return false;
  result.setInt32(int32_t(height ? value : width));
  return true;
}
}
bool WindowGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int32_t slot = 0;
  State* state = Receiver(cx, args, &slot);
  if (!state) return false;
  const char* member = WindowMemberName(slot);
  if (member[0]) state->Observe("Window", member, int(ApiAccess::Read));
  switch (slot) {
    case kWindowName:
      args.rval().set(state->name);
      return true;
    case kWindowOrigin:
      return state->ReadOrigin(args.rval());
    case kWindowIsSecureContext: {
      bool secure;
      if (!state->ReadSecureContext(&secure)) return false;
      args.rval().setBoolean(secure); return true;
    }
    case kWindowCrossOriginIsolated:
      args.rval().setBoolean(state->IsCrossOriginIsolated());
      return true;
    case kWindowInnerWidth: return ViewportField(*state, false, args.rval());
    case kWindowInnerHeight: return ViewportField(*state, true, args.rval());
    case kWindowOuterWidth: return ScreenField(*state, 0, args.rval());
    case kWindowOuterHeight: return ScreenField(*state, 1, args.rval());
    case kWindowScreenX: return ScreenField(*state, 4, args.rval());
    case kWindowScreenY: return ScreenField(*state, 5, args.rval());
    case kWindowMozInnerScreenX: return InnerScreenField(*state, false, args.rval());
    case kWindowMozInnerScreenY: return InnerScreenField(*state, true, args.rval());
    case kWindowExternal:
      args.rval().setObject(*state->external);
      return JS_WrapValue(cx, args.rval());
    case kWindowVisualViewport:
      args.rval().setObject(*state->visualViewport);
      return JS_WrapValue(cx, args.rval());
    case kWindowPageXOffset:
    case kWindowScrollX:
      args.rval().setDouble(state->scrollX);
      return true;
    case kWindowPageYOffset:
    case kWindowScrollY:
      args.rval().setDouble(state->scrollY);
      return true;
    case kWindowDevicePixelRatio:
      // The one device pixel ratio source: the profile's viewport group, which
      // the styling owner also reads for `resolution` and
      // `-webkit-device-pixel-ratio`. A child realm reads its parent's model.
      return state->Read(Kind::Viewport, kViewportDevicePixelRatio,
                         ValueType::Number, args.rval());
    case kWindowClosed:
      args.rval().setBoolean(state->closed);
      return true;
    case kWindowLength: {
      size_t count = state->frameCount ? state->frameCount(state->frameCountContext) : 0;
      args.rval().setInt32(count > size_t(std::numeric_limits<int32_t>::max())
                               ? std::numeric_limits<int32_t>::max() : int32_t(count));
      return true;
    }
    case kWindowFrames:
      args.rval().setObject(*state->page);
      return JS_WrapValue(cx, args.rval());
    default:
      return Error(cx, "Window member is not installed");
  }
}
bool WindowSetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int32_t slot = 0;
  State* state = Receiver(cx, args, &slot);
  if (!state) return false;
  const char* member = WindowMemberName(slot);
  if (member[0]) state->Observe("Window", member, int(ApiAccess::Write));
  switch (slot) {
    case kWindowInnerWidth:
    case kWindowInnerHeight: {
      JSAutoRealm realm(cx, state->page);
      JS::RootedValue value(cx, args.get(0));
      if (!JS_WrapValue(cx, &value) ||
          !JS_DefineProperty(cx, state->page, member, value, JSPROP_ENUMERATE)) return false;
      break;
    }
    case kWindowName: {
      JS::RootedString text(cx, JS::ToString(cx, args.get(0)));
      if (!text) return false;
      state->name = JS::StringValue(text);
      break;
    }
    case kWindowVisualViewport: {
      // [Replaceable]: assignment defines an own data property on this window,
      // exactly like the donor's set_visualViewport binding. The receiver check
      // above already resolved null/undefined to this window.
      JS::RootedObject target(cx, state->page);
      JS::RootedValue value(cx, args.get(0));
      if (!JS_DefineProperty(cx, target, "visualViewport", value, JSPROP_ENUMERATE))
        return false;
      break;
    }
    case kWindowScrollX:
    case kWindowScrollY: {
      double value = 0.0;
      if (!JS::ToNumber(cx, args.get(0), &value)) return false;
      if (!std::isfinite(value)) break;  // Firefox ignores a non-finite scroll target.
      if (slot == kWindowScrollX) state->scrollX = value;
      else state->scrollY = value;
      break;
    }
    default:
      return Error(cx, "Window member is read-only");
  }
  args.rval().setUndefined();
  return true;
}
// WindowOrWorkerGlobalScope encoding members. btoa maps each Latin-1 code unit
// to a byte; atob runs the forgiving-base64 decode and returns one code unit
// per decoded byte. Both report spec failures as InvalidCharacterError.
namespace {
const char kBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
int Base64Value(char16_t unit) {
  if (unit >= u'A' && unit <= u'Z') return unit - u'A';
  if (unit >= u'a' && unit <= u'z') return unit - u'a' + 26;
  if (unit >= u'0' && unit <= u'9') return unit - u'0' + 52;
  if (unit == u'+') return 62;
  if (unit == u'/') return 63;
  return -1;
}
bool AsciiSpace(char16_t unit) {
  return unit == 0x09 || unit == 0x0a || unit == 0x0c || unit == 0x0d || unit == 0x20;
}
bool InvalidCharacter(State& state) {
  return ThrowDOM(state, "InvalidCharacterError", "The string to be decoded is not correctly encoded");
}
bool Btoa(State& state, const JS::CallArgs& args, JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  JS::RootedString input(cx, JS::ToString(cx, args.get(0)));
  if (!input) return false;
  size_t length = JS_GetStringLength(input);
  std::string encoded;
  encoded.reserve((length + 2) / 3 * 4);
  uint32_t block = 0;
  size_t pending = 0;
  for (size_t index = 0; index < length; ++index) {
    char16_t unit = 0;
    if (!JS_GetStringCharAt(cx, input, index, &unit)) return false;
    if (unit > 0xff) return InvalidCharacter(state);
    block = (block << 8) | unit;
    if (++pending == 3) {
      encoded.push_back(kBase64[(block >> 18) & 0x3f]);
      encoded.push_back(kBase64[(block >> 12) & 0x3f]);
      encoded.push_back(kBase64[(block >> 6) & 0x3f]);
      encoded.push_back(kBase64[block & 0x3f]);
      block = 0;
      pending = 0;
    }
  }
  if (pending == 1) {
    block <<= 16;
    encoded.push_back(kBase64[(block >> 18) & 0x3f]);
    encoded.push_back(kBase64[(block >> 12) & 0x3f]);
    encoded.push_back('=');
    encoded.push_back('=');
  } else if (pending == 2) {
    block <<= 8;
    encoded.push_back(kBase64[(block >> 18) & 0x3f]);
    encoded.push_back(kBase64[(block >> 12) & 0x3f]);
    encoded.push_back(kBase64[(block >> 6) & 0x3f]);
    encoded.push_back('=');
  }
  JS::RootedString text(cx, JS_NewStringCopyN(cx, encoded.data(), encoded.size()));
  if (!text) return false;
  result.setString(text);
  return true;
}
bool Atob(State& state, const JS::CallArgs& args, JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  JS::RootedString input(cx, JS::ToString(cx, args.get(0)));
  if (!input) return false;
  size_t length = JS_GetStringLength(input);
  std::u16string data;
  data.reserve(length);
  for (size_t index = 0; index < length; ++index) {
    char16_t unit = 0;
    if (!JS_GetStringCharAt(cx, input, index, &unit)) return false;
    if (!AsciiSpace(unit)) data.push_back(unit);
  }
  if (data.size() % 4 == 0) {
    if (data.size() >= 2 && data[data.size() - 2] == u'=' && data.back() == u'=')
      data.resize(data.size() - 2);
    else if (!data.empty() && data.back() == u'=') data.resize(data.size() - 1);
  }
  if (data.size() % 4 == 1) return InvalidCharacter(state);
  for (char16_t unit : data)
    if (Base64Value(unit) < 0) return InvalidCharacter(state);
  std::u16string decoded;
  decoded.reserve(data.size() / 4 * 3);
  uint32_t block = 0;
  size_t pending = 0;
  for (char16_t unit : data) {
    block = (block << 6) | uint32_t(Base64Value(unit));
    if (++pending == 4) {
      decoded.push_back(char16_t((block >> 16) & 0xff));
      decoded.push_back(char16_t((block >> 8) & 0xff));
      decoded.push_back(char16_t(block & 0xff));
      block = 0;
      pending = 0;
    }
  }
  if (pending == 2) {
    decoded.push_back(char16_t((block >> 4) & 0xff));
  } else if (pending == 3) {
    decoded.push_back(char16_t((block >> 10) & 0xff));
    decoded.push_back(char16_t((block >> 2) & 0xff));
  }
  JS::RootedString text(cx, JS_NewUCStringCopyN(cx, decoded.data(), decoded.size()));
  if (!text) return false;
  result.setString(text);
  return true;
}
// A transfer list is undefined/null or an array; anything else is a TypeError.
bool TransferList(JSContext* cx, JS::HandleValue value, JS::MutableHandleValue result) {
  result.set(JS::UndefinedValue());
  if (value.isUndefined() || value.isNull()) return true;
  bool array = false;
  if (!JS::IsArrayObject(cx, value, &array)) return false;
  if (!array) return Error(cx, "transfer must be an array");
  result.set(value);
  return true;
}
bool StructuredClone(State& state, const JS::CallArgs& args, JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  JS::RootedValue transfer(cx, JS::UndefinedValue());
  if (args.length() >= 2 && !args[1].isUndefined()) {
    if (!args[1].isObject()) return Error(cx, "structuredClone options must be an object");
    JS::RootedObject options(cx, &args[1].toObject());
    bool present = false;
    JS::RootedValue value(cx);
    if (!JS_HasProperty(cx, options, "transfer", &present)) return false;
    if (present && !JS_GetProperty(cx, options, "transfer", &value)) return false;
    if (present && !TransferList(cx, value, &transfer)) return false;
  }
  JS::RootedValue source(cx, args.get(0));
  return CloneValue(state, source, transfer, result);
}
}  // namespace
bool WindowEncoding(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int32_t slot = 0;
  State* state = Receiver(cx, args, &slot);
  if (!state) return false;
  state->Observe("Window", WindowMemberName(slot), int(ApiAccess::Call));
  if (slot == kWindowBtoa) return Btoa(*state, args, args.rval());
  if (slot == kWindowAtob) return Atob(*state, args, args.rval());
  return Error(cx, "Window encoding member is not installed");
}
// The selected WindowOrWorkerGlobalScope members on WorkerGlobalScope.prototype:
// the receiver is the Worker global (or undefined/null, the callee's global).
bool WorkerEncoding(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  const int32_t slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  if (!state || !state->worker || (!args.thisv().isNullOrUndefined() &&
      (!args.thisv().isObject() || &args.thisv().toObject() != state->page)))
    return WebKit::FailBrand(cx, args, "WorkerGlobalScope");
  state->Observe("WorkerGlobalScope", WindowMemberName(slot), int(ApiAccess::Call));
  if (slot == kWindowBtoa) return Btoa(*state, args, args.rval());
  if (slot == kWindowAtob) return Atob(*state, args, args.rval());
  if (slot == kWindowStructuredClone) return StructuredClone(*state, args, args.rval());
  return Error(cx, "Worker encoding member is not installed");
}
bool WindowCloning(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int32_t slot = 0;
  State* state = Receiver(cx, args, &slot);
  if (!state) return false;
  if (slot != kWindowStructuredClone)
    return Error(cx, "Window cloning member is not installed");
  state->Observe("Window", "structuredClone", int(ApiAccess::Call));
  return StructuredClone(*state, args, args.rval());
}
struct WindowAccessorSpec { const char* name; int32_t slot; bool writable; };
// Canonical install-time member names of the Window facade, shared by the
// installer and the debug monitor so every member is named once.
// screenLeft/screenTop reuse the screenX/screenY slots, and the first entry of
// a slot is the primary name the monitor reports.
static const WindowAccessorSpec kWindowAccessors[] = {
  {"name", kWindowName, true},
  {"origin", kWindowOrigin, false},
  {"isSecureContext", kWindowIsSecureContext, false},
  {"crossOriginIsolated", kWindowCrossOriginIsolated, false},
  {"innerWidth", kWindowInnerWidth, true},
  {"innerHeight", kWindowInnerHeight, true},
  {"outerWidth", kWindowOuterWidth, false},
  {"outerHeight", kWindowOuterHeight, false},
  {"screenX", kWindowScreenX, false},
  {"screenY", kWindowScreenY, false},
  {"screenLeft", kWindowScreenX, false},
  {"screenTop", kWindowScreenY, false},
  {"pageXOffset", kWindowPageXOffset, false},
  {"pageYOffset", kWindowPageYOffset, false},
  {"devicePixelRatio", kWindowDevicePixelRatio, false},
  {"scrollX", kWindowScrollX, true},
  {"scrollY", kWindowScrollY, true},
  {"closed", kWindowClosed, false},
  {"length", kWindowLength, false},
  {"frames", kWindowFrames, false},
  {"mozInnerScreenX", kWindowMozInnerScreenX, false},
  {"mozInnerScreenY", kWindowMozInnerScreenY, false},
  {"external", kWindowExternal, false},
  {"visualViewport", kWindowVisualViewport, true},
};
// length is the donor's Function.length: the number of required arguments. It
// is part of the installed function's shape, so it travels with the table
// instead of being hardcoded by the installer.
struct WindowMethodSpec { const char* name; int32_t slot; JSNative native; unsigned length; };
static const WindowMethodSpec kWindowMethods[] = {
  {"alert", kWindowAlert, WindowMethod, 0}, {"close", kWindowClose, WindowMethod, 0},
  {"open", kWindowOpen, WindowMethod, 0}, {"scrollTo", kWindowScrollTo, WindowMethod, 0},
  {"scrollBy", kWindowScrollBy, WindowMethod, 0}, {"scroll", kWindowScroll, WindowMethod, 0},
  {"btoa", kWindowBtoa, WindowEncoding, 1}, {"atob", kWindowAtob, WindowEncoding, 1},
  {"structuredClone", kWindowStructuredClone, WindowCloning, 1},
  {"postMessage", kWindowPostMessage, WindowMessaging, 1},
  {"confirm", kWindowConfirm, WindowMethod, 0},
  {"prompt", kWindowPrompt, WindowMethod, 0},
  {"print", kWindowPrint, WindowMethod, 0},
  {"stop", kWindowStop, WindowMethod, 0},
  {"focus", kWindowFocus, WindowMethod, 0},
  {"blur", kWindowBlur, WindowMethod, 0},
  {"moveTo", kWindowMoveTo, WindowMethod, 2},
  {"moveBy", kWindowMoveBy, WindowMethod, 2},
  {"resizeTo", kWindowResizeTo, WindowMethod, 2},
  {"resizeBy", kWindowResizeBy, WindowMethod, 2},
  {"captureEvents", kWindowCaptureEvents, WindowMethod, 0},
  {"releaseEvents", kWindowReleaseEvents, WindowMethod, 0},
  {"reportError", kWindowReportError, WindowMethod, 1},
};
const char* WindowMemberName(int32_t slot) {
  for (const auto& spec : kWindowAccessors)
    if (spec.slot == slot) return spec.name;
  for (const auto& spec : kWindowMethods)
    if (spec.slot == slot) return spec.name;
  return "";
}
bool InstallWindowMembers(State& state) {
  // The replaceable window.name attribute defaults to the empty string, not
  // undefined; every later assignment goes through ToString in the setter.
  if (state.name.isUndefined()) {
    JS::RootedString empty(state.cx, JS_NewStringCopyZ(state.cx, ""));
    if (!empty) return false;
    state.name = JS::StringValue(empty);
  }
  for (const auto& spec : kWindowAccessors) {
    if (!state.Accessor(state.page, spec.name, spec.slot, JSPROP_ENUMERATE,
                        WindowGetter, spec.writable ? WindowSetter : nullptr) ||
        !state.helpers->Accessor("Window", state.page, spec.name)) return false;
  }
  for (const auto& spec : kWindowMethods) {
    JS::RootedObject method(state.cx,
        state.Function(spec.native, spec.name, spec.length, spec.slot));
    if (!method ||
        !JS_DefineProperty(state.cx, state.page, spec.name, method, JSPROP_ENUMERATE) ||
        !state.helpers->Member(method, "Window", spec.name, "call")) return false;
  }
  return true;
}
bool ThrowDOM(State& state, const char* name, const char* message) {
  if (state.throwException)
    return state.throwException(state.exceptionContext, name, message);
  return Error(state.cx, message, JSEXN_ERR);
}
bool State::WindowMembers() { return InstallWindowMembers(*this); }
bool State::InstallWorkerEncoding(JS::HandleObject prototype) {
  for (const auto& spec : kWindowMethods) {
    if (spec.native != WindowEncoding && spec.native != WindowCloning) continue;
    JS::RootedObject method(cx, Function(WorkerEncoding, spec.name, spec.length, spec.slot));
    if (!method || !JS_DefineProperty(cx, prototype, spec.name, method, JSPROP_ENUMERATE) ||
        !helpers->Member(method, "WorkerGlobalScope", spec.name, "call")) return false;
  }
  return true;
}
bool Environment::InnerScreenOrigin(double* x, double* y) {
  JSAutoRealm realm(state_->cx, state_->page);
  return InnerScreen(*state_, x, y);
}
}
