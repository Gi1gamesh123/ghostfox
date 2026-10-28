#include "web_audio/state.h"

#include <cmath>

#include "js/Conversions.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"
#include "js/ErrorReport.h"
#include "js/Object.h"
#include "js/String.h"

namespace ruyijs::web_audio {
namespace {

const char* kReceiver = "Web Audio member called on an incompatible receiver";

bool ParamId(JSContext* cx, JS::HandleValue self, uint32_t* id) {
  JS::RootedObject object(cx);
  if (!WebKit::Check(cx, self, &kParamClass, &object, kReceiver)) return false;
  *id = static_cast<uint32_t>(JS::GetReservedSlot(object, 0).toInt32());
  return true;
}

// Every automation method takes its time at argument 1; the donor ran them all
// through one Set behaviour, so the only added step A13.3 requires is rejecting
// a time that is not a non-negative finite number with a RangeError.
bool AutomationTime(JSContext* cx, const JS::CallArgs& args, double* time) {
  const unsigned position=WebKit::Slot(args)>=5?0:1;
  if (args.length() <= position)
    return WebKit::Fail(cx, "AudioParam automation requires a time value");
  if (!JS::ToNumber(cx, args[position], time)) return false;
  if (!std::isfinite(*time) || *time < 0)
    return WebKit::FailKind(cx, "The time provided is negative or not finite",
                            JSEXN_RANGEERR);
  return true;
}

}  // namespace

JSObject* NewParam(State& state, const ParamSpec& spec) {
  const uint32_t id = state.model.create_param(spec);
  if (id == 0) {
    JS_ReportOutOfMemory(state.cx);
    return nullptr;
  }
  JS::RootedObject object(state.cx, state.kit.New(&kParamClass, state.param.prototype));
  if (!object) return nullptr;
  JS::SetReservedSlot(object, 0, JS::Int32Value(static_cast<int32_t>(id)));
  return object.get();
}

bool ParamGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = static_cast<State*>(WebKit::Owner(args));
  uint32_t id = 0;
  if (!ParamId(cx, args.thisv(), &id)) return false;
  double number = 0.0;
  switch (WebKit::Slot(args)) {
    case 0:
      if (!state->model.param_value(id, &number)) return WebKit::Fail(cx, kReceiver);
      args.rval().setNumber(number);
      return true;
    case 1:
      if (!state->model.param_default(id, &number)) return WebKit::Fail(cx, kReceiver);
      args.rval().setNumber(number);
      return true;
    case 2:
      if (!state->model.param_min(id, &number)) return WebKit::Fail(cx, kReceiver);
      args.rval().setNumber(number);
      return true;
    case 3:
      if (!state->model.param_max(id, &number)) return WebKit::Fail(cx, kReceiver);
      args.rval().setNumber(number);
      return true;
    default:
      return WebKit::Fail(cx, kReceiver);
  }
}

// The value setter and the six automation methods share the donor's single
// Set behaviour: clamp through the model, then return the receiver.
bool ParamSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = static_cast<State*>(WebKit::Owner(args));
  uint32_t id = 0;
  if (!ParamId(cx, args.thisv(), &id)) return false;
  double number = 0.0;
  if (args.length() && !JS::ToNumber(cx, args.get(0), &number)) return false;
  if (!state->model.set_param_value(id, number)) return WebKit::Fail(cx, kReceiver);
  args.rval().set(args.thisv());
  return true;
}

// The automation entry point: validate the time, then record the clamped value
// in the domain aggregate (the single owner of parameter state) and return the
// receiver, exactly like the value setter.
bool ParamAutomate(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = static_cast<State*>(WebKit::Owner(args));
  uint32_t id = 0;
  if (!ParamId(cx, args.thisv(), &id)) return false;
  double time = 0.0;
  if (!AutomationTime(cx, args, &time)) return false;
  const int32_t operation=WebKit::Slot(args);
  if(operation>=5) {args.rval().set(args.thisv());return true;}
  double number = 0.0;
  JS::RootedValue value(cx,args.get(0));
  if(operation==4 && value.isObject()) {
    JS::RootedObject curve(cx,&value.toObject());
    uint32_t length=0;
    if(!JS::GetArrayLength(cx,curve,&length)) return false;
    if(length==0) return WebKit::Fail(cx,"AudioParam curve requires values");
    if(!JS_GetElement(cx,curve,length-1,&value)) return false;
  }
  if (!JS::ToNumber(cx, value, &number)) return false;
  if (!state->model.set_param_value(id, number)) return WebKit::Fail(cx, kReceiver);
  args.rval().set(args.thisv());
  return true;
}

}  // namespace ruyijs::web_audio
