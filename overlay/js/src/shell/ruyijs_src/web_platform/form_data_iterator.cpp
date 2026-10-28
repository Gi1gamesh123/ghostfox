// The FormData iterator. It holds the FormData object and a position; every step
// re-reads the one entry array, so a mutation between steps is observed the way the
// specification requires. entries yields [name, value], keys the name and values the
// value, and the iterator itself is iterable.
#include "web_platform/state.h"

#include "runtime/web_kit.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::web_platform {
namespace {
enum IteratorSlot { kIteratorSource = 0, kIteratorIndex, kIteratorKind, kIteratorSlots };
}  // namespace

const JSClass kFormDataIteratorClass = {
    "FormData Iterator", JSCLASS_HAS_RESERVED_SLOTS(kIteratorSlots)};

bool FormDataIterator(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  if (!FormDataBrand(cx, args.thisv(), &self)) return false;
  JS::RootedObject object(
      cx, state.kit.New(&kFormDataIteratorClass, state.formDataIteratorPrototype));
  if (!object) return false;
  JS::SetReservedSlot(object, kIteratorSource, JS::ObjectValue(*self));
  JS::SetReservedSlot(object, kIteratorIndex, JS::Int32Value(0));
  JS::SetReservedSlot(object, kIteratorKind, JS::Int32Value(WebKit::Slot(args)));
  args.rval().setObject(*object);
  return true;
}

// @@iterator returns the receiver, so for-of and Array.from work on it.
bool FormDataIdentity(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!args.thisv().isObject() ||
      JS::GetClass(&args.thisv().toObject()) != &kFormDataIteratorClass)
    return WebKit::Fail(cx, "FormData iterator called on an incompatible receiver");
  args.rval().set(args.thisv());
  return true;
}

bool FormDataNext(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!args.thisv().isObject() ||
      JS::GetClass(&args.thisv().toObject()) != &kFormDataIteratorClass)
    return WebKit::Fail(cx, "FormData iterator called on an incompatible receiver");
  JS::RootedObject iterator(cx, &args.thisv().toObject());
  JS::RootedValue source(cx, JS::GetReservedSlot(iterator, kIteratorSource));
  if (!source.isObject()) return WebKit::Fail(cx, "FormData iterator is not initialized");
  JS::RootedObject form_data(cx, &source.toObject());
  JS::RootedObject list(cx);
  if (!FormDataList(cx, form_data, &list)) return false;
  uint32_t length = 0;
  bool array = false;
  if (!JS::IsArrayObject(cx, list, &array) || !array || !JS::GetArrayLength(cx, list, &length))
    return false;
  const uint32_t index = uint32_t(JS::GetReservedSlot(iterator, kIteratorIndex).toInt32());
  const FormDataIteratorKind kind =
      FormDataIteratorKind(JS::GetReservedSlot(iterator, kIteratorKind).toInt32());
  JS::RootedObject result(cx, JS_NewPlainObject(cx));
  JS::RootedValue value(cx), done(cx, JS::BooleanValue(index >= length));
  if (!result) return false;
  if (index < length) {
    JS::RootedValue entry(cx), name(cx), item(cx);
    if (!JS_GetElement(cx, list, index, &entry) || !entry.isObject()) return false;
    JS::RootedObject pair(cx, &entry.toObject());
    if (!JS_GetElement(cx, pair, 0, &name) || !JS_GetElement(cx, pair, 1, &item)) return false;
    if (kind == kFormDataEntries) {
      JS::RootedObject tuple(cx, JS::NewArrayObject(cx, 2));
      if (!tuple || !JS_SetElement(cx, tuple, 0, name) || !JS_SetElement(cx, tuple, 1, item))
        return false;
      value.setObject(*tuple);
    } else {
      value.set(kind == kFormDataKeys ? name : item);
    }
    JS::SetReservedSlot(iterator, kIteratorIndex, JS::Int32Value(int32_t(index + 1)));
  }
  if (!JS_DefineProperty(cx, result, "done", done, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, result, "value", value, JSPROP_ENUMERATE))
    return false;
  args.rval().setObject(*result);
  return true;
}
}  // namespace ruyijs::web_platform
