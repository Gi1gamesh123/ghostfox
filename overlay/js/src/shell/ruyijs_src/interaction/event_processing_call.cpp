// The public ordinary-call side effect belongs to this realm's Event adapter.
#include "interaction/internal.h"
#include "js/GlobalObject.h"
#include "js/Wrapper.h"

namespace ruyijs::interaction {
bool CallProcessingEvent(JSContext* cx, const JS::CallArgs& args, Iface iface) {
  if (args.length() < PublicEventArity(iface))
    return Error(cx, "TypeError", "Event requires a type and options");
  JS::RootedString type(cx);
  std::string unused;
  if (!PublicFormattedEventType(cx, args[0], &type, &unused)) return false;
  bool bubbles, cancelable, composed;
  if (!ReadPublicEventBase(cx, iface, args.get(1), &bubbles, &cancelable, &composed)) return false;
  JS::RootedObject dict(cx, args.get(1).isObject() ? &args[1].toObject() : nullptr);
  JS::RootedObject temporary(cx, JS_NewObject(cx, &kEventClass));
  if (!temporary || !InitProcessingFields(cx, iface, dict, temporary)) return false;
  JS::RootedObject receiver(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (args.thisv().isNullOrUndefined()) receiver = JS::GetNonCCWObjectGlobal(&args.callee());
  for (State* state = activeState; state; state = state->previous)
    if (state->cx == cx && state->page == receiver) state->publicWindowTrusted = true;
  args.rval().setUndefined();
  return true;
}
bool ReadPublicWindowTrusted(JSContext* cx, const JS::CallArgs& args, bool cross_realm) {
  JS::RootedObject receiver(cx, args.thisv().isObject() ? &args.thisv().toObject() : nullptr);
  if (receiver && cross_realm) receiver = js::CheckedUnwrapStatic(receiver);
  if (args.thisv().isNullOrUndefined()) receiver = JS::GetNonCCWObjectGlobal(&args.callee());
  for (State* state = activeState; state; state = state->previous) {
    if (state->cx == cx && state->page == receiver && state->publicWindowTrusted) {
      state->Observe("Event", "isTrusted", int(ApiAccess::Read));
      args.rval().setBoolean(false);
      return true;
    }
  }
  return false;
}
}  // namespace ruyijs::interaction
