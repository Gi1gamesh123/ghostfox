#include "interaction/internal.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "jsfriendapi.h"

namespace ruyijs::interaction {
void Realm::SetAbortPorts(const AbortPorts& ports) { state_->abortPorts = ports; }
// The abort steps of one listener: remove exactly that listener (by id) from
// its target, so a later add of the same callback is unaffected.
static bool AbortListener(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  // rval() aliases the callee slot: read the callee's reserved slots first.
  JS::RootedObject callee(cx, &args.callee());
  JS::RootedValue object(cx, js::GetFunctionNativeReserved(callee, 0));
  const JS::Value id = js::GetFunctionNativeReserved(callee, 1);
  js::SetFunctionNativeReserved(callee, 0, JS::UndefinedValue());
  args.rval().setUndefined();
  if (!object.isObject() || !id.isNumber()) return true;
  JS::RootedObject receiver(cx, &object.toObject());
  State* state = Owner(cx, receiver, false);
  Target* target = state ? state->target(receiver) : nullptr;
  if (!target) return true;
  target->aggregate.remove_listener(domain::ListenerId(id.toNumber()));
  state->cleanup();
  return true;
}
// AddEventListenerOptions.signal: a non-nullable AbortSignal member.
static bool Signal(JSContext* cx, State& state, JS::HandleValue value) {
  if (value.isUndefined()) return true;
  if (!value.isObject())
    return Error(cx, "TypeError", "EventTarget.addEventListener: 'signal' member of AddEventListenerOptions is not an object.");
  if (!state.abortPorts.brand) return Error(cx, "TypeError", "AbortSignal owner is not installed");
  if (!state.abortPorts.brand(value))
    return Error(cx, "TypeError", "EventTarget.addEventListener: 'signal' member of AddEventListenerOptions does not implement interface AbortSignal.");
  return true;
}
static bool Options(JSContext* cx, State& state, JS::HandleValue value, bool adding,
                    domain::ListenerOptions* options, JS::MutableHandleValue signal) {
  if (!value.isObject()) { options->capture = JS::ToBoolean(value); return true; }
  JS::RootedObject object(cx, &value.toObject());
  if (!Option(cx, object, "capture", &options->capture)) return false;
  if (!adding) return true;
  if (!Option(cx, object, "once", &options->once) ||
      !Option(cx, object, "passive", &options->passive)) return false;
  if (!JS_GetProperty(cx, object, "signal", signal)) return false;
  return Signal(cx, state, signal);
}
static bool Change(JSContext* cx, unsigned argc, JS::Value* vp, bool adding) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject receiver(cx);
  State* state = nullptr;
  Target* target = TargetReceiver(cx, args, &receiver, &state);
  if (!target) return false;
  // Observation only, after the receiver check: a non-EventTarget throws above
  // and records nothing.
  state->Observe("EventTarget", adding ? "addEventListener" : "removeEventListener",
                 int(ApiAccess::Call));
  if (argc < 2) return Error(cx, "TypeError", "Event listener requires type and callback");
  JS::RootedString text(cx);
  std::string type;
  if (!Text(cx, args[0], &text, &type)) return false;
  if (!args[1].isNullOrUndefined() && !args[1].isObject())
    return Error(cx, "TypeError", "Event listener must be an object or null");
  domain::ListenerOptions options;
  JS::RootedValue signal(cx);
  if (!Options(cx, *state, args.get(2), adding, &options, &signal)) return false;
  if (adding && !args.get(3).isNullOrUndefined())
    options.wants_untrusted = JS::ToBoolean(args[3]);
  args.rval().setUndefined();
  if (args[1].isNullOrUndefined()) return true;
  if (adding && !signal.isUndefined() && state->abortPorts.aborted(signal)) return true;
  JS::RootedObject callback(cx, &args[1].toObject());
  state->cleanup();
  domain::ListenerId existing = 0;
  for (auto& listener : state->listeners) {
    if (listener->target == target->aggregate.id() && listener->type == type &&
        listener->capture == options.capture && listener->callback == callback) {
      domain::EventListener entry(listener->id, domain::EventType(), {});
      if (adding && (!target->aggregate.listener(listener->id, &entry) ||
          entry.options().wants_untrusted != options.wants_untrusted)) continue;
      existing = listener->id; break;
    }
  }
  if (!adding) {
    if (existing) target->aggregate.remove_listener(existing);
    state->cleanup();
    return true;
  }
  if (existing) return true;
  if (state->listeners.size() >= 100000) return Error(cx, "RangeError", "Event listener limit exceeded");
  auto id = target->aggregate.add_listener(domain::EventType(type), options);
  auto listener = std::make_shared<Listener>(cx, callback, target->aggregate.id(), id,
                                            std::move(type), options.capture);
  state->listeners.push_back(listener);
  state->callbacks.emplace(id, [state, listener](domain::DomEvent& event, uint64_t owner) {
    state->invoke(listener, event, owner);
  });
  if (signal.isUndefined()) return true;
  JSFunction* steps = js::NewFunctionWithReserved(cx, AbortListener, 0, 0, nullptr);
  if (!steps) return false;
  JS::RootedObject algorithm(cx, JS_GetFunctionObject(steps));
  js::SetFunctionNativeReserved(algorithm, 0, JS::ObjectValue(*receiver));
  js::SetFunctionNativeReserved(algorithm, 1, JS::DoubleValue(double(id)));
  JS::RootedValue value(cx, JS::ObjectValue(*algorithm));
  return state->abortPorts.follow(cx, signal, value);
}
bool Add(JSContext* cx, unsigned n, JS::Value* vp) { return Change(cx, n, vp, true); }
bool Remove(JSContext* cx, unsigned n, JS::Value* vp) { return Change(cx, n, vp, false); }
void State::cleanup() {
  listeners.erase(std::remove_if(listeners.begin(), listeners.end(), [this](const auto& value) {
    auto target = targetMap.find(value->target);
    domain::EventListener entry(value->id, domain::EventType(), {});
    if (target != targetMap.end() && target->second->listener(value->id, &entry)) return false;
    callbacks.erase(value->id);
    return true;
  }), listeners.end());
}
}
