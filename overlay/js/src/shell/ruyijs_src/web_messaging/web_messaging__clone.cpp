#include "web_messaging/state.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/ForOfIterator.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::web_messaging {
bool TransferOptions(JSContext* cx, JS::HandleValue options, JS::MutableHandleValue output) {
  output.setUndefined();
  if (options.isNullOrUndefined()) return true;
  if (!options.isObject()) return WebKit::Fail(cx, "Transfer options require an object");
  JS::ForOfIterator iterator(cx);
  if (!iterator.init(options, JS::ForOfIterator::AllowNonIterable)) return false;
  if (!iterator.valueIsIterable()) {
    JS::RootedObject dictionary(cx, &options.toObject());
    JS::RootedValue transfer(cx);
    if (!JS_GetProperty(cx, dictionary, "transfer", &transfer)) return false;
    if (transfer.isUndefined()) return true;
    if (!transfer.isObject()) return WebKit::Fail(cx, "Transfer must be a sequence of objects");
    if (!iterator.init(transfer)) return false;
  }
  JS::RootedObject list(cx, JS::NewArrayObject(cx, 0));
  if (!list) return false;
  for (uint32_t index = 0;; ++index) {
    JS::RootedValue item(cx); bool done = false;
    if (!iterator.next(&item, &done)) return false;
    if (done) break;
    if (!item.isObject()) return WebKit::Fail(cx, "Transfer entries must be objects");
    if (index >= 65536) return WebKit::Fail(cx, "Transfer list limit exceeded");
    if (!JS_DefineElement(cx, list, index, item, JSPROP_ENUMERATE)) return false;
  }
  output.setObject(*list); return true;
}
bool Message::Write(State& sender, JS::HandleValue value, JS::HandleValue transfer) {
  if (buffer.write(sender.cx, value, transfer, JS::CloneDataPolicy(), &cloneCallbacks, this)) return true;
  if (!error && JS_IsExceptionPending(sender.cx)) return false;
  JS_ClearPendingException(sender.cx);
  return CloneError(sender);
}
bool Message::Read(State& target, JS::MutableHandleValue value) {
  destination = &target;
  JSAutoRealm realm(target.cx, target.kit.page);
  ports = JS::NewArrayObject(target.cx, 0);
  if (!ports) return false;
  if (buffer.read(target.cx, value, JS::CloneDataPolicy(), &cloneCallbacks, this)) return true;
  if (!error && JS_IsExceptionPending(target.cx)) return false;
  JS_ClearPendingException(target.cx);
  return CloneError(target);
}
bool Realm::Clone(JS::HandleValue source, JS::HandleValue transfer, JS::HandleObject destination,
                  JS::MutableHandleValue result, JS::MutableHandleObject ports) {
  State* target = activeState;
  while (target && target->kit.page != destination) target = target->previous;
  if (!target) return WebKit::Fail(state_->cx, "Message destination realm is not installed");
  Message message(state_->cx);
  if (!message.Write(*state_, source, transfer) || !message.Read(*target, result)) return false;
  ports.set(message.ports);
  return JS_WrapValue(state_->cx, result) && JS_WrapObject(state_->cx, ports);
}
}
