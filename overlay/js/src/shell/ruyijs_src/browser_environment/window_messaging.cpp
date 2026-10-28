#include "browser_environment/browser_chrome.h"
#include "browser_environment/cross_origin.h"
#include "browser_environment/window_clone.h"
#include "browser_environment/window_members.h"
#include "browser_environment/window_message_options.h"
#include "domain/browser_environment/url_value.h"
#include "runtime/runtime.h"
#include "jsfriendapi.h"
#include "js/Object.h"
#include "js/String.h"

namespace ruyijs::browser_environment {
namespace {
enum MessageSlot { Data, Source, Origin, TargetOrigin, Ports, SlotCount };
const JSClass kMessageClass = {"WindowMessageTask", JSCLASS_HAS_RESERVED_SLOTS(SlotCount)};
bool DeliverMessage(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  if (!state || !state->messageActive) { args.rval().setUndefined(); return true; }
  if (!state->deliverMessage) return Error(cx, "Message delivery is not bound");
  JS::RootedObject packet(cx, &js::GetFunctionNativeReserved(&args.callee(), 1).toObject());
  args.rval().setUndefined();
  JS::RootedValue expected(cx, JS::GetReservedSlot(packet, TargetOrigin)), current(cx);
  if (!expected.isUndefined()) {
    if (!state->ReadOrigin(&current)) return false;
    JS::RootedString a(cx, expected.toString()), b(cx, current.toString());
    int32_t comparison = 0;
    if (!JS_CompareStrings(cx, a, b, &comparison)) return false;
    // Opaque origins never compare equal by their "null" serialization.
    bool opaque = false;
    if (!JS_StringEqualsLiteral(cx, a, "null", &opaque)) return false;
    if (comparison || opaque) return true;
  }
  JS::RootedValue data(cx, JS::GetReservedSlot(packet, Data));
  JS::RootedObject source(cx, &JS::GetReservedSlot(packet, Source).toObject());
  JS::RootedObject sourcePrototype(cx);
  State* sourceOwner = WindowGlobalOwner(cx, source, &sourcePrototype);
  if (!sourceOwner) return false;
  if (!sourceOwner->messageActive) source = nullptr;
  // MessageEvent.source is the sender's WindowProxy as this window sees it.
  JS::RootedValue sourceView(cx);
  if (source) {
    if (!WindowView(cx, state->page, source, &sourceView)) return false;
    source = &sourceView.toObject();
  }
  JS::RootedString originText(cx, JS::GetReservedSlot(packet, Origin).toString());
  std::string origin;
  if (!ToUTF8(cx, originText, &origin)) return false;
  JS::RootedObject ports(cx, &JS::GetReservedSlot(packet, Ports).toObject());
  return state->deliverMessage(state->deliverMessageContext, state->page, data, origin.c_str(), source, ports);
}
}
bool WindowMessaging(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject prototype(cx);
  State* target = WindowOwner(cx, args, &prototype);
  if (!target) return false;
  auto* callee = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  target->Observe("Window", "postMessage", int(ApiAccess::Call));
  JS::RootedObject sender(cx, JS::GetScriptedCallerGlobal(cx));
  if (!sender) sender = JS::GetNonCCWObjectGlobal(&args.callee());
  State* source = WindowGlobalOwner(cx, sender, &prototype);
  if (!source) return false;
  std::string requested;
  JS::RootedValue transfer(cx), origin(cx), expected(cx), message(cx);
  if (!MessageOptions(cx, args, &requested, &transfer) || !source->ReadOrigin(&origin)) return false;
  if (requested == "/") expected = origin;
  else if (requested != "*") {
    ruyijs::domain::browser_environment::UrlValue parsed;
    if (!ruyijs::domain::browser_environment::UrlValue::Resolve(requested, "", &parsed))
      return ThrowDOM(*callee, "SyntaxError", "Invalid target origin");
    std::string value = parsed.origin();
    JS::RootedString text(cx, JS_NewStringCopyN(cx, value.data(), value.size()));
    if (!text) return false;
    expected.setString(text);
  }
  JS::RootedObject ports(cx);
  if (!CloneValue(*callee, args.get(0), transfer, &message, target->page, &ports)) return false;
  if (!target->messageActive) { args.rval().setUndefined(); return true; }
  if (!target->postTask || !target->postMessageTask) return Error(cx, "postMessage requires the scheduling owner");
  JSAutoRealm realm(cx, target->page);
  if (!JS_WrapObject(cx, &sender) || !JS_WrapValue(cx, &message) ||
      !JS_WrapValue(cx, &origin) || !JS_WrapValue(cx, &expected) || !JS_WrapObject(cx, &ports)) return false;
  JS::RootedObject packet(cx, JS_NewObjectWithGivenProto(cx, &kMessageClass, nullptr));
  if (!packet) return false;
  JS::SetReservedSlot(packet, Data, message);
  JS::SetReservedSlot(packet, Source, JS::ObjectValue(*sender));
  JS::SetReservedSlot(packet, Origin, origin);
  JS::SetReservedSlot(packet, TargetOrigin, expected);
  JS::SetReservedSlot(packet, Ports, JS::ObjectValue(*ports));
  JS::RootedFunction function(cx, js::NewFunctionWithReserved(cx, DeliverMessage, 0, 0, "postMessage"));
  if (!function) return false;
  JS::RootedObject task(cx, JS_GetFunctionObject(function));
  js::SetFunctionNativeReserved(task, 0, JS::PrivateValue(target));
  js::SetFunctionNativeReserved(task, 1, JS::ObjectValue(*packet));
  if (!target->postMessageTask(target->postMessageTaskContext, task, sender,
                               target->page, ++source->messageSequence, 0)) return false;
  args.rval().setUndefined();
  return true;
}
}
