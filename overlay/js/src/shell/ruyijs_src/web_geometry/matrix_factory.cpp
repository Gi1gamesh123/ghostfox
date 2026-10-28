#include "web_geometry/matrix.h"
#include "runtime/native_constructor_trace.h"
#include "js/Array.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_geometry {
const JSClass kMatrixClass = {"DOMMatrix", JSCLASS_HAS_RESERVED_SLOTS(17), nullptr};
const JSClass kMatrixReadOnlyClass = {"DOMMatrixReadOnly", JSCLASS_HAS_RESERVED_SLOTS(17), nullptr};
bool MatrixReceiver(JSContext* cx, JS::HandleValue value, bool readonly, JS::MutableHandleObject out) {
  out.set(value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  const JSClass* clasp = out ? JS::GetClass(out) : nullptr;
  return clasp == &kMatrixClass || (readonly && clasp == &kMatrixReadOnlyClass) ||
         WebKit::Fail(cx, "DOMMatrix member called on an incompatible receiver");
}
void ReadMatrix(JS::HandleObject object, double* values) {
  for (unsigned i = 0; i < 16; ++i) values[i] = JS::GetReservedSlot(object, i).toNumber();
}
bool NewMatrix(State& s, bool readonly, const double* values, bool is2D,
                const JS::CallArgs* args, JS::MutableHandleValue result) {
  WebFace& face = readonly ? s.matrixReadOnly : s.matrix;
  JS::RootedObject proto(s.cx, face.prototype);
  if (args && !s.kit.ConstructProto(*args, face.prototype, &proto)) return false;
  JS::RootedObject object(s.cx, s.kit.New(face.clasp, proto));
  if (!object) return false;
  for (unsigned i = 0; i < 16; ++i) JS::SetReservedSlot(object, i, JS::NumberValue(values[i]));
  JS::SetReservedSlot(object, 16, JS::BooleanValue(is2D));
  result.setObject(*object);
  return true;
}
bool MatrixConstruct(JSContext* cx, unsigned argc, JS::Value* vp, bool readonly) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing()) return WebKit::Fail(cx, "DOMMatrix constructor requires new");
  double values[16]; domain::geometry::MatrixIdentity(values);
  // Preserve the original mutable constructor's six-number array/dictionary
  // capability. The newly introduced ReadOnly constructor is the public stub.
  if (!readonly && args.get(0).isObject()) {
    JS::RootedObject init(cx, &args[0].toObject());
    bool array;
    if (!JS::IsArrayObject(cx, init, &array)) return false;
    for (unsigned i = 0; i < 6; ++i) {
      JS::RootedValue value(cx);
      if (!(array ? JS_GetElement(cx, init, i, &value) : JS_GetProperty(cx, init, kMatrixFields[i], &value))) return false;
      if (!value.isNumber()) return WebKit::Fail(cx, "DOMMatrix init requires six numeric components");
      values[domain::geometry::kAliases[i]] = value.toNumber();
    }
  }
  return trace.Finish(NewMatrix(s, readonly, values, true, &args, args.rval()));
}
bool MatrixFrom(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  bool readonly = WebKit::Slot(args) == 0;
  double values[16]; domain::geometry::MatrixIdentity(values);
  return NewMatrix(s, readonly, values, true, nullptr, args.rval());
}
bool Realm::MatrixFrom2D(const double components[6], JS::MutableHandleObject result) {
  State& s = *state_;
  JS::RootedValue value(s.cx);
  {
    JSAutoRealm realm(s.cx, s.kit.page);
    double values[16]; domain::geometry::MatrixIdentity(values);
    for (unsigned i = 0; i < 6; ++i) values[domain::geometry::kAliases[i]] = components[i];
    if (!NewMatrix(s, false, values, true, nullptr, &value)) return false;
  }
  if (!JS_WrapValue(s.cx, &value)) return false;
  result.set(&value.toObject());
  return true;
}
}
