#include "web_gpu/internal.h"
#include "web_gpu/value_objects.h"
#include "js/MapAndSet.h"
#include "js/Realm.h"

namespace ruyijs::web_gpu {
namespace {
const std::vector<std::string>& Source(State& state, JS::HandleObject self) {
  const int source = JS::GetReservedSlot(self, kFeaturesSource).toInt32();
  return source == kWgslFeaturesSource ? state.model.wgsl_features()
       : source == 1 ? state.model.device_features() : state.model.adapter_features();
}
State* Receiver(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleObject self) {
  return ValueReceiver(cx, args.thisv(), WebKit::Slot(args) >= kWgslMemberBase
                       ? &kWgslFeaturesClass : &kFeaturesClass, self);
}
bool ReturnIterator(State& state, const std::vector<std::string>& features, bool entries,
                    JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  // The domain owns the readonly values. This temporary Set only supplies the
  // engine's native iterator and keeps its data alive for that iterator's life.
  {
    JSAutoRealm realm(cx, state.kit.page);
    JS::RootedObject set(cx, JS::NewSetObject(cx));
    if (!set) return false;
    for (const auto& feature : features) {
      JS::RootedValue value(cx);
      if (!StringValue(cx, feature, &value) || !JS::SetAdd(cx, set, value)) return false;
    }
    if (!(entries ? JS::SetEntries(cx, set, result) : JS::SetValues(cx, set, result))) return false;
  }
  return JS_WrapValue(cx, result);
}
}

JSObject* NewFeatures(State& state, int32_t source) {
  const JSAutoRealm realm(state.cx, state.kit.page);
  WebFace& face = source == kWgslFeaturesSource ? state.wgsl_features : state.features;
  JS::RootedObject features(state.cx, state.kit.New(face.clasp, face.prototype));
  if (!features) return nullptr;
  JS::SetReservedSlot(features, kFeaturesSource, JS::Int32Value(source));
  return features.get();
}

bool WgslFeaturesProperty(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  State* state = ValueReceiver(cx, args.thisv(), &kGpuClass, &self);
  if (!state) return false;
  {
    JSAutoRealm realm(cx, self);
    JS::RootedValue cached(cx, JS::GetReservedSlot(self, 0));
    if (!cached.isObject()) {
      JS::RootedObject features(cx, NewFeatures(*state, kWgslFeaturesSource));
      if (!features) return false;
      cached.setObject(*features);
      JS::SetReservedSlot(self, 0, cached);
    }
    args.rval().set(cached);
  }
  return JS_WrapValue(cx, args.rval());
}

bool FeaturesProperty(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  State* state = Receiver(cx, args, &self);
  if (!state) return false;
  args.rval().setNumber(double(Source(*state, self).size()));
  return true;
}

bool FeaturesMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  State* state = Receiver(cx, args, &self);
  if (!state) return false;
  const auto& features = Source(*state, self);
  switch (WebKit::Slot(args) % kWgslMemberBase) {
    case 0: {
      JS::RootedString wanted(cx, JS::ToString(cx, args.get(0)));
      std::string text;
      if (!wanted || !ToUTF8(cx, wanted, &text)) return false;
      for (const auto& feature : features) {
        if (feature == text) { args.rval().setBoolean(true); return true; }
      }
      args.rval().setBoolean(false);
      return true;
    }
    case 1:
      return ReturnIterator(*state, features, true, args.rval());
    case 2:
    case 3:
      return ReturnIterator(*state, features, false, args.rval());
    default: {
      JS::RootedValue callback(cx, args.get(0));
      if (!callback.isObject() || !JS::IsCallable(&callback.toObject()))
        return WebKit::Fail(cx, "forEach requires a callback");
      JS::RootedValue receiver(cx, args.get(1));
      for (const auto& feature : features) {
        JS::RootedValueArray<3> call(cx);
        if (!StringValue(cx, feature, call[0])) return false;
        call[1].set(call[0]);
        call[2].set(args.thisv());
        JS::RootedValue ignored(cx);
        if (!JS::Call(cx, receiver, callback, call, &ignored)) return false;
      }
      args.rval().setUndefined();
      return true;
    }
  }
}
}
