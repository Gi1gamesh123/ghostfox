// Navigator members of the Navigator core interface with fixed or
// document-derived answers: javaEnabled/taintEnabled (constant false,
// Navigator.webidl) and registerProtocolHandler, whose validation is the donor
// Navigator::CheckProtocolHandlerAllowed (domain/browser_environment/
// protocol_handler.h). A headless runtime has no handler registry or prompt, so
// an accepted registration is observable only as returning undefined, as a
// Firefox content caller sees it.
#include "browser_environment/environment.h"
#include "browser_environment/schema.h"
#include "browser_environment/state.h"
#include "domain/browser_environment/protocol_handler.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "runtime/runtime.h"

namespace ruyijs::browser_environment {
namespace domain = ::ruyijs::domain::browser_environment;
namespace {
State* Navigator(const JS::CallArgs& args, const char* member) {
  State* state = ReceiverState(args, ClassFor(Kind::Navigator));
  if (state) state->Observe("Navigator", member, int(ApiAccess::Call));
  return state;
}
bool Fail(JSContext* cx, const std::string& message) {
  return Error(cx, message.c_str());
}
bool ConstantFalse(JSContext* cx, unsigned argc, JS::Value* vp, const char* member) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!Navigator(args, member)) return ReceiverError(cx, args, "Navigator");
  args.rval().setBoolean(false);
  return true;
}
bool JavaEnabled(JSContext* cx, unsigned argc, JS::Value* vp) {
  return ConstantFalse(cx, argc, vp, "javaEnabled");
}
bool TaintEnabled(JSContext* cx, unsigned argc, JS::Value* vp) {
  return ConstantFalse(cx, argc, vp, "taintEnabled");
}
bool RegisterProtocolHandler(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = Navigator(args, "registerProtocolHandler");
  if (!state) return ReceiverError(cx, args, "Navigator");
  if (args.length() < 2)
    return Fail(cx, "Navigator.registerProtocolHandler: At least 2 arguments required, but only " +
                    std::to_string(args.length()) + " passed");
  JS::RootedString schemeText(cx, JS::ToString(cx, args[0]));
  std::string scheme, input, base;
  if (!schemeText || !ToUTF8(cx, schemeText, &scheme)) return false;
  JS::RootedString urlText(cx, JS::ToString(cx, args[1]));
  if (!urlText || !ToUTF8(cx, urlText, &input)) return false;
  JS::RootedValue current(cx);
  if (!state->Read(Kind::Location, 0, ValueType::String, &current)) return false;
  JS::RootedString currentText(cx, current.toString());
  if (!currentText || !ToUTF8(cx, currentText, &base)) return false;
  domain::UrlValue document, handler;
  if (!domain::UrlValue::Resolve(base, "", &document) ||
      !domain::UrlValue::Resolve(input, base, &handler))
    return ThrowDOM(*state, "SyntaxError", "An invalid or illegal string was specified");
  const std::string prefix = "Navigator.registerProtocolHandler: ";
  switch (domain::CheckProtocolHandler(scheme, handler, document)) {
    case domain::ProtocolHandlerCheck::MissingPlaceholder:
      return ThrowDOM(*state, "SyntaxError",
                      (prefix + "Handler URI does not contain \"%s\".").c_str());
    case domain::ProtocolHandlerCheck::DeniedHandler:
      return ThrowDOM(*state, "SecurityError", (prefix + "Permission denied to add " +
                      handler.href() + " as a protocol handler").c_str());
    case domain::ProtocolHandlerCheck::DeniedScheme:
      return ThrowDOM(*state, "SecurityError",
                      (prefix + "Permission denied to add a protocol handler for " + scheme).c_str());
    case domain::ProtocolHandlerCheck::Allowed:
      break;
  }
  args.rval().setUndefined();
  return true;
}
struct Spec { const char* name; JSNative native; unsigned length; };
const Spec kMembers[] = {
    {"javaEnabled", JavaEnabled, 0},
    {"taintEnabled", TaintEnabled, 0},
    {"registerProtocolHandler", RegisterProtocolHandler, 2},
};
}  // namespace

bool State::NavigatorLegacyMembers() {
  JS::RootedObject prototype(cx, navigatorPrototype);
  if (!prototype) return false;
  for (const Spec& spec : kMembers) {
    JS::RootedObject method(cx, Function(spec.native, spec.name, spec.length, 0));
    if (!method || !JS_DefineProperty(cx, prototype, spec.name, method, JSPROP_ENUMERATE) ||
        !helpers->Member(method, "Navigator", spec.name, "call")) return false;
  }
  return true;
}

bool Environment::ConfigureSecureMembers() {
  State& s = *state_;
  JS::RootedObject prototype(s.cx, s.navigatorPrototype), page(s.cx, s.page);
  bool secure = false;
  if (!prototype || !s.ReadSecureContext(&secure)) return false;
  return s.protocolHandlerSlot.Apply(s.cx, prototype, secure) &&
         s.storageSlot.Apply(s.cx, prototype, secure) &&
         s.storageManagerSlot.Apply(s.cx, page, secure) &&
         s.rawUpdateSlot.Apply(s.cx, page, secure) &&
         s.pipInterfaceSlot.Apply(s.cx, page, secure) && s.pipWindowSlot.Apply(s.cx, page, secure);
}
}  // namespace ruyijs::browser_environment
