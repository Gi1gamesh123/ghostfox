#include "runtime/host_bridge.h"
#include "runtime/pipe.h"
#include "runtime/typed_decode.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CharacterEncoding.h"
#include "js/Exception.h"
#include "js/PropertyAndElement.h"
#include "js/PropertyDescriptor.h"
#include "js/Realm.h"
#include "js/String.h"

namespace ruyijs {
HostBridge::HostBridge(JSContext* cx, JS::HandleObject page,
                       JS::HandleObject codec, Pipe& pipe, Checkpoint checkpoint, PageHelpers& helpers)
    : cx_(cx), page_(cx, page), codec_(cx, codec), data_(cx), helpers_(&helpers), functions_(cx, cx),
      pipe_(pipe), checkpoint_(checkpoint) {}

HostBridge::~HostBridge() {
  healthy_ = false;
  JS::AutoSaveExceptionState saved(cx_);
  checkpoint_(cx_);
  for (JSObject* function : functions_.get())
    js::SetFunctionNativeReserved(function, 0, JS::PrivateValue(nullptr));
  saved.restore();
}

bool HostBridge::Name(JS::HandleString name, JS::MutableHandleId id,
                       JS::MutableHandleString localName) {
  if (JS_GetStringLength(name) > Pipe::kMaxFrame) {
    JS_ReportErrorASCII(cx_, "Host binding name exceeds protocol limit");
    return false;
  }
  JS::RootedValue wrapped(cx_, JS::StringValue(name));
  if (!JS_WrapValue(cx_, &wrapped)) return false;
  localName.set(wrapped.toString());
  if (!JS_StringToId(cx_, localName, id)) return false;
  JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> descriptor(cx_);
  if (!JS_GetOwnPropertyDescriptorById(cx_, page_, id, &descriptor)) return false;
  if (descriptor.isSome() && !descriptor->configurable()) {
    JS_ReportErrorASCII(cx_, "Host binding cannot replace a nonconfigurable property");
    return false;
  }
  return true;
}

bool HostBridge::Expose(JS::HandleString name) {
  JSAutoRealm realm(cx_, page_);
  JS::RootedId id(cx_);
  JS::RootedString localName(cx_);
  if (!Name(name, &id, &localName)) return false;
  JS::RootedObject object(cx_);
  return MakeCallback(localName, localName, &object) &&
         JS_DefinePropertyById(cx_, page_, id, object, JSPROP_ENUMERATE) &&
         Record(Kind::Function, name, nullptr, false, JS::UndefinedHandleValue);
}

bool HostBridge::Set(JS::HandleString name, JS::HandleValue encoded) {
  JSAutoRealm realm(cx_, page_);
  JS::RootedId id(cx_);
  JS::RootedString localName(cx_);
  JS::RootedValue input(cx_, encoded), value(cx_);
  return Name(name, &id, &localName) && JS_WrapValue(cx_, &input) &&
         DecodeTyped(cx_, input, &value) &&
         JS_DefinePropertyById(cx_, page_, id, value, JSPROP_ENUMERATE);
}

bool HostBridge::Native(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  auto* bridge = static_cast<HostBridge*>(
      js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  if (!bridge || !bridge->healthy()) {
    JS_ReportErrorASCII(cx, "Host callback session is closed");
    return false;
  }
  return bridge->Call(args) || bridge->PageException();
}
}
