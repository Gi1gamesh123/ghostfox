#include "web_audio/node_fields.h"
#include <cmath>

#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Realm.h"

namespace ruyijs::web_audio {
namespace {

State* Self(const JS::CallArgs& args) {
  return static_cast<State*>(WebKit::Owner(args));
}

// AudioListener keeps its own JSClass because setPosition/setOrientation store
// coordinates in reserved slots. Firefox's IDL exposes no getters, so the
// slots are write-only, exactly like the donor's AudioListener::mPosition.
bool ListenerReceiver(JSContext* cx, const JS::CallArgs& args,
                      JS::MutableHandleObject self) {
  if (!args.thisv().isObject() ||
      JS::GetClass(&args.thisv().toObject()) != &kListenerClass)
    return WebKit::Fail(cx, "AudioListener member called on an incompatible receiver");
  self.set(&args.thisv().toObject());
  return true;
}

// WebIDL float conversion finishes before any coordinate state is mutated.
bool StoreFloats(JSContext* cx, const JS::CallArgs& args,
                 JS::HandleObject self, int32_t first, int32_t count) {
  if (args.length() < size_t(count))
    return WebKit::Fail(cx, "AudioListener requires all coordinates");
  double values[6];
  for (int32_t index = 0; index < count; ++index) {
    if (!JS::ToNumber(cx, args[index], &values[index])) return false;
    values[index] = double(float(values[index]));
    if (!std::isfinite(values[index]))
      return WebKit::Fail(cx, "AudioListener coordinates must be finite floats");
  }
  for (int32_t index = 0; index < count; ++index) {
    JS::RootedObject param(cx,&JS::GetReservedSlot(self,first+index).toObject());
    const auto id=uint32_t(JS::GetReservedSlot(param,0).toInt32());
    if(!Self(args)->model.set_param_value(id,values[index])) return false;
  }
  return true;
}

}  // namespace

// PeriodicWave is constructible; Listener and Worklet use illegal constructors.
bool PlainCtor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!args.isConstructing()) return WebKit::Fail(cx, "PeriodicWave requires 'new'");
  if (!detail::IsContext(cx, args.get(0))) return false;
  State* state = Self(args);
  JS::RootedObject proto(cx);
  if (!state->kit.ConstructProto(args, state->periodic.prototype, &proto)) return false;
  JS::RootedObject object(cx, state->kit.New(nullptr, proto));
  if (!object) return false;
  args.rval().setObject(*object);
  return true;
}

bool ListenerSetPosition(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!ListenerReceiver(cx, args, &self) || !StoreFloats(cx, args, self, 0, 3))
    return false;
  args.rval().setUndefined();
  return true;
}

bool ListenerSetOrientation(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!ListenerReceiver(cx, args, &self) || !StoreFloats(cx, args, self, 3, 6))
    return false;
  args.rval().setUndefined();
  return true;
}

bool Noop(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  args.rval().setUndefined();
  return true;
}

bool InitListener(State& s,JS::HandleObject listener) {
  for(int32_t i=0;i<9;++i) {
    const double initial=i==5?-1:i==7?1:0;
    JS::RootedObject param(s.cx,NewParam(s,{initial,-3.4028234663852886e38,
                                          3.4028234663852886e38,true}));
    if(!param) return false;
    JS::SetReservedSlot(listener,i,JS::ObjectValue(*param));
  }
  return true;
}
bool WorkletPort(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject self(cx);
  if(!WebKit::Check(cx,args.thisv(),&kWorkletClass,&self,
                   "AudioWorklet member called on an incompatible receiver")) return false;
  // The MessagePort factory belongs to the pending Worklet execution owner.
  args.rval().setNull();return true;
}

}  // namespace ruyijs::web_audio
