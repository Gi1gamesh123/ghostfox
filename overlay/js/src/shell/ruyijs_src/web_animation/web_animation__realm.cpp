// The Web Animations realm: State, shared helpers and the one installer
// (docs/owners/WEB_ANIMATION.md).
#include "web_animation/state.h"
#include "runtime/page_helpers.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/String.h"
#include "js/WeakMap.h"

namespace ruyijs::web_animation {
const char* const kCompositeNames[3] = {"replace", "add", "accumulate"};
const char* const kHandlerNames[3] = {"finish", "cancel", "remove"};

State::State(JSContext* c, JS::HandleObject page, PageHelpers& helpers, interaction::Realm& e)
    : cx(c), kit(c, page, helpers, this), events(e), timeline(c), documentTimeline(c), effect(c),
      keyframeEffect(c), animation(c), cssAnimation(c), cssTransition(c), timelines(c),
      playbackEvent(c), domException(c), relevant(c) {}

State& OwnerOf(JSObject* object) {
  return *static_cast<State*>(JS::GetReservedSlot(object, 0).toPrivate());
}
bool IsAnimation(JSObject* object) {
  const JSClass* clasp = object ? JS::GetClass(object) : nullptr;
  return clasp == &kAnimationClass || clasp == &kCssAnimationClass || clasp == &kCssTransitionClass;
}
bool TypeFail(JSContext* cx, const std::string& message) { return WebKit::Fail(cx, message.c_str()); }
bool NotImplemented(JSContext* cx, const char* member, const char* iface) {
  return TypeFail(cx, std::string("'") + member +
                      "' called on an object that does not implement interface " + iface + ".");
}
bool DomFail(State& s, const char* name, const std::string& message) {
  JSContext* cx = s.cx;
  JS::RootedValueArray<2> values(cx);
  JS::RootedObject error(cx);
  if (!NewString(cx, message, values[0]) || !NewString(cx, name, values[1]) ||
      !JS::Construct(cx, s.domException, values, &error)) return false;
  JS::RootedValue value(cx, JS::ObjectValue(*error));
  JS_SetPendingException(cx, value);
  return false;
}
bool RejectPending(State& s, JS::MutableHandleValue rval) {
  JS::RootedValue error(s.cx);
  if (!JS_GetPendingException(s.cx, &error)) return false;
  JS_ClearPendingException(s.cx);
  return s.kit.RejectWith(error, rval);
}
bool Text(JSContext* cx, JS::HandleValue value, std::string* out) {
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text) return false;
  JS::UniqueChars bytes = JS_EncodeStringToUTF8(cx, text);
  if (!bytes) return false;
  *out = bytes.get();
  return true;
}
std::string NumberText(JSContext* cx, double value) {
  JS::RootedValue number(cx, JS::NumberValue(value));
  std::string out;
  if (!Text(cx, number, &out)) { JS_ClearPendingException(cx); return "NaN"; }
  return out;
}
bool NewString(JSContext* cx, const std::string& text, JS::MutableHandleValue out) {
  JSString* str = JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(text.data(), text.size()));
  if (!str) return false;
  out.setString(str);
  return true;
}
JSObject* NewTask(State& s, JSNative native, JS::HandleObject target) {
  JSFunction* function = js::NewFunctionWithReserved(s.cx, native, 0, 0, "Animation task");
  if (!function) return nullptr;
  JSObject* object = JS_GetFunctionObject(function);
  js::SetFunctionNativeReserved(object, 0, JS::PrivateValue(&s));
  js::SetFunctionNativeReserved(object, 1, JS::ObjectValue(*target));
  return object;
}

Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, interaction::Realm& events)
    : state_(std::make_unique<State>(cx, page, helpers, events)) {}
Realm::~Realm() = default;
void Realm::SetDocumentPorts(const DocumentPorts& ports) { state_->dom = ports; }
void Realm::SetSchedulingPorts(const SchedulingPorts& ports) { state_->scheduling = ports; state_->tick.reset(); }

bool Realm::Install(JS::HandleObject documentPrototype, JS::HandleObject elementPrototype) {
  State& s = *state_;
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.kit.page);
  if (!documentPrototype || !elementPrototype || !s.dom.kind)
    return TypeFail(cx, "Web Animations requires the Document Model ports");
  // Install time, before any page script: the realm's own event and error interfaces.
  if (!JS_GetProperty(cx, s.kit.page, "AnimationPlaybackEvent", &s.playbackEvent) ||
      !JS_GetProperty(cx, s.kit.page, "DOMException", &s.domException) ||
      !s.playbackEvent.isObject() || !s.domException.isObject())
    return TypeFail(cx, "Web Animations requires AnimationPlaybackEvent and DOMException");
  s.timelines = JS::NewWeakMapObject(cx);
  if (!s.timelines) return false;
  return InstallTimelines(s, documentPrototype) && InstallEffects(s) && InstallAnimations(s) &&
         InstallElementMembers(s, elementPrototype) ;
}
}  // namespace ruyijs::web_animation
