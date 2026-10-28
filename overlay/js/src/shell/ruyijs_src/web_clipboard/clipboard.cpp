// navigator.clipboard and the Clipboard interface (dom/webidl/Clipboard.webidl,
// donor dom/events/Clipboard.cpp). This runtime has no system clipboard and its
// user-activation baseline is inactive, so after WebIDL argument conversion
// every operation rejects with the donor's activation NotAllowedError
// (Clipboard::ReadHelper / Write "blocked due to lack of user activation").
#include "web_clipboard/internal.h"
#include "browser_environment/environment.h"
#include "interaction/realm.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_clipboard {
const JSClass kClipboardClass = {"Clipboard", 0};
namespace {
enum Op { kRead = 0, kReadText, kWrite, kWriteText };
const char* const kNames[] = {"read", "readText", "write", "writeText"};
const unsigned kLengths[] = {0, 0, 1, 1};

bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return trace.Finish(WebKit::Fail(cx, "Illegal constructor."));
}
bool Operation(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  const int op = WebKit::Slot(args);
  JS::RootedObject self(cx);
  const std::string member = kNames[op];
  if (!WebKit::Check(cx, args.thisv(), &kClipboardClass, &self,
                     ("'" + member + "' called on an object that does not implement interface Clipboard.").c_str()))
    return RejectPending(s, args.rval());
  if (argc < kLengths[op]) {
    (void)WebKit::Fail(cx, ("Clipboard." + member + ": At least 1 argument required, but only 0 passed").c_str());
    return RejectPending(s, args.rval());
  }
  if (op == kWriteText) {
    JS::RootedString text(cx, JS::ToString(cx, args[0]));
    if (!text) return RejectPending(s, args.rval());
  }
  // write(sequence<ClipboardItem>) converts its argument before the activation
  // check; the conversion is the existing ClipboardItem brand.
  if (op == kWrite) {
    bool array = false;
    if (!args[0].isObject() || !JS::IsArrayObject(cx, args[0], &array) || !array) {
      (void)WebKit::Fail(cx, "Clipboard.write: Argument 1 can't be converted to a sequence.");
      return RejectPending(s, args.rval());
    }
  }
  JS::RootedValue error(cx);
  const std::u16string message = op <= kReadText
      ? u"Clipboard read request was blocked due to lack of user activation."
      : u"Clipboard write was blocked due to lack of user activation.";
  return NewDomException(s, message, "NotAllowedError", &error) && s.kit.RejectWith(error, args.rval());
}
bool NavigatorClipboard(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedValue navigator(cx);
  JSObject* self = args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr;
  {
    JSAutoRealm realm(cx, s.kit.page);
    if (!JS_GetProperty(cx, s.kit.page, "navigator", &navigator)) return false;
  }
  if (!self || !navigator.isObject() ||
      JS::GetClass(self) != JS::GetClass(js::UncheckedUnwrap(&navigator.toObject())))
    return WebKit::Fail(cx, "'get clipboard' called on an object that does not implement interface Navigator.");
  JS::RootedValue value(cx, JS::ObjectValue(*s.clipboard));
  if (!JS_WrapValue(cx, &value)) return false;
  args.rval().set(value);
  return true;
}
}  // namespace

bool InstallClipboard(State& s, JS::HandleObject eventTarget, JS::HandleObject navigatorPrototype) {
  JSContext* cx = s.cx;
  if (!s.kit.Define("Clipboard", &kClipboardClass, eventTarget, Illegal, &s.clipboardFace)) return false;
  for (int op = kRead; op <= kWriteText; ++op)
    if (!s.kit.Method(s.clipboardFace, "Clipboard", kNames[op], Operation, kLengths[op], op)) return false;
  s.clipboard = s.kit.New(&kClipboardClass, s.clipboardFace.prototype);
  JS::RootedObject clipboard(cx, s.clipboard);
  JS::RootedObject getter(cx, s.kit.Function(NavigatorClipboard, "get clipboard", 0, 0)), setter(cx);
  return clipboard && getter && s.attach(s.attachContext, clipboard) &&
         JS_DefineProperty(cx, navigatorPrototype, "clipboard", getter, setter, JSPROP_ENUMERATE) &&
         s.kit.helpers->Accessor("Navigator", navigatorPrototype, "clipboard") &&
         s.secure.Add(navigatorPrototype, "clipboard") && s.secure.Add(s.kit.page, "Clipboard") &&
         s.secure.Add(s.kit.page, "ClipboardItem");
}
}  // namespace ruyijs::web_clipboard
