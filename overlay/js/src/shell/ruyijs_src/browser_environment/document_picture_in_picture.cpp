// DocumentPictureInPicture and window.documentPictureInPicture ([SecureContext],
// docs/owners/BROWSER_ENVIRONMENT.md, 2026-10-21). requestWindow follows
// Firefox's checks (top-level, transient activation consumed, width/height
// together); this headless runtime has no window system, so a request that
// passes them rejects as Firefox does when the PiP window cannot be created.
#include "browser_environment/window_legacy.h"
#include "browser_environment/window_members.h"
#include "interaction/realm.h"
#include "runtime/page_helpers.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::browser_environment {
namespace {
const JSClass kPipClass = {"DocumentPictureInPicture", JSCLASS_HAS_RESERVED_SLOTS(1)};
enum Member { mRequestWindow, mWindow, mGetOnEnter, mSetOnEnter };
bool Rejected(WindowLegacyState& w, const char* name, const char* message, JS::MutableHandleValue out) {
  JSContext* cx = w.env.cx;
  JS::RootedValue error(cx);
  if (!ThrowDOM(w.env, name, message) && JS_GetPendingException(cx, &error)) JS_ClearPendingException(cx);
  else if (!error.isObject()) return false;
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!promise || !JS::RejectPromise(cx, promise, error)) return false;
  out.setObject(*promise);
  return true;
}
bool Member(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  WindowLegacyState& w = *static_cast<WindowLegacyState*>(WebKit::Owner(args));
  const int member = WebKit::Slot(args);
  JSObject* self = args.thisv().isObject() ? &args.thisv().toObject() : nullptr;
  if (!self || JS::GetClass(self) != &kPipClass) {
    const char* names[] = {"requestWindow", "get window", "get onenter", "set onenter"};
    return WebKit::Fail(cx, (std::string("'") + names[member] +
                             "' called on an object that does not implement interface DocumentPictureInPicture.").c_str());
  }
  switch (member) {
    case mWindow: args.rval().setNull(); return true;
    case mGetOnEnter: args.rval().set(w.onenter); return true;
    case mSetOnEnter: {  // EventHandler: one registration through EventTarget's own methods
      JS::RootedValue previous(cx, w.onenter), desired(cx, args.get(0)), method(cx), ignored(cx);
      if (!desired.isObject() || !JS::IsCallable(&desired.toObject())) desired.setNull();
      JS::RootedObject proto(cx, w.events.prototype());
      JS::RootedValueArray<2> params(cx);
      JS::RootedString type(cx, JS_NewStringCopyZ(cx, "enter"));
      if (!type) return false;
      params[0].setString(type);
      JS::RootedValue target(cx, JS::ObjectValue(*self));
      for (int add = 0; add < 2; ++add) {
        params[1].set(add ? desired : previous);
        if (params[1].isObject() && (!JS_GetProperty(cx, proto, add ? "addEventListener" : "removeEventListener", &method) ||
                                     !JS::Call(cx, target, method, params, &ignored))) return false;
      }
      w.onenter = desired;
      args.rval().setUndefined();
      return true;
    }
    default: break;
  }
  // requestWindow(optional DocumentPictureInPictureOptions options = {})
  double width = 0, height = 0;
  if (!args.get(0).isNullOrUndefined()) {
    if (!args[0].isObject())
      return WebKit::Fail(cx, "DocumentPictureInPicture.requestWindow: Argument 1 can't be converted to a dictionary.");
    JS::RootedObject options(cx, &args[0].toObject());
    JS::RootedValue v(cx);
    for (const char* name : {"disallowReturnToOpener", "height", "preferInitialWindowPlacement", "width"}) {
      if (!JS_GetProperty(cx, options, name, &v)) return false;
      if (v.isUndefined()) continue;
      double number = 0;
      if (name[0] == 'h' || name[0] == 'w') {
        if (!JS::ToNumber(cx, v, &number)) return false;
        if (!std::isfinite(number) || number < 0 || number > 9007199254740991.0)
          return WebKit::Fail(cx, (std::string("DocumentPictureInPicture.requestWindow: '") + name +
                                   "' member of DocumentPictureInPictureOptions is outside the range for an unsigned long long.").c_str());
        (name[0] == 'h' ? height : width) = std::trunc(number);
      } else {
        (void)JS::ToBoolean(v);
      }
    }
  }
  if (w.env.shared)
    return Rejected(w, "NotAllowedError", "Document Picture-in-Picture is only available in top-level contexts", args.rval());
  const bool active = w.env.activationClock &&
      w.env.activation.Transient(w.env.activationClock(w.env.activationClockContext));
  if (!active) return Rejected(w, "NotAllowedError", "Document Picture-in-Picture requires user activation", args.rval());
  w.env.activation.ConsumeTransient();
  if ((width > 0) != (height > 0))
    return Rejected(w, "RangeError", "requestWindow: width and height must be specified together", args.rval());
  return Rejected(w, "UnknownError", "Failed to create PIP window", args.rval());
}
}  // namespace

bool InstallDocumentPictureInPicture(WindowLegacyState& w) {
  JSContext* cx = w.env.cx;
  JS::RootedObject target(cx, w.events.prototype());
  if (!w.kit.Define("DocumentPictureInPicture", &kPipClass, target, WebKit::Reject, &w.pip) ||
      !JS_DeleteProperty(cx, w.pip.prototype, "constructor") ||
      !w.kit.Method(w.pip, "DocumentPictureInPicture", "requestWindow", Member, 0, mRequestWindow) ||
      !w.kit.Accessor(w.pip, "DocumentPictureInPicture", "window", Member, nullptr, mWindow)) return false;
  JS::RootedObject proto(cx, w.pip.prototype);
  JS::RootedObject get(cx, w.kit.Function(Member, "get onenter", 0, mGetOnEnter));
  JS::RootedObject set(cx, w.kit.Function(Member, "set onenter", 1, mSetOnEnter));
  if (!get || !set || !JS_DefineProperty(cx, proto, "onenter", get, set, JSPROP_ENUMERATE) ||
      !w.env.helpers->Accessor("DocumentPictureInPicture", proto, "onenter") ||
      !JS_DefineProperty(cx, proto, "constructor", w.pip.constructor, 0)) return false;
  w.pipInstance = w.kit.New(&kPipClass, proto);
  return w.pipInstance && w.events.Attach(w.pipInstance);
}
}  // namespace ruyijs::browser_environment
