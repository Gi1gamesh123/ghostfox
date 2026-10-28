#include "document_model/range.h"
#include "document_model/state.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
// Immutable boundary values trace the original wrappers, including foreign
// realm wrappers. No node keys or independent tree state are copied here.
const JSClass kStaticRangeClass = {"StaticRange", JSCLASS_HAS_RESERVED_SLOTS(4), nullptr};

namespace {
bool NodeValue(State& state, JS::HandleValue value) {
  JS::RootedObject node(state.cx, value.isObject()
      ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  auto* wrapper = node && JS::GetClass(node) == &kNodeClass
      ? static_cast<Wrapper*>(JS::GetReservedSlot(node, 0).toPrivate()) : nullptr;
  if (!wrapper || wrapper->dead || !wrapper->owner)
    return TypeError(state.cx, "StaticRange container must be a Node");
  return true;
}
}

bool CreateStaticRange(State& state, const JS::CallArgs& args) {
  if (!args.get(0).isObject()) return TypeError(state.cx, "StaticRange requires a dictionary");
  JS::RootedObject init(state.cx, &args[0].toObject());
  JS::RootedValueArray<4> values(state.cx);
  const char* names[] = {"startContainer", "startOffset", "endContainer", "endOffset"};
  for (unsigned i = 0; i < 4; ++i) {
    bool present = false;
    if (!JS_HasProperty(state.cx, init, names[i], &present)) return false;
    if (!present) return TypeError(state.cx, "StaticRange dictionary member is missing");
    if (!JS_GetProperty(state.cx, init, names[i], values[i])) return false;
    if (i % 2 == 0) {
      if (!NodeValue(state, values[i])) return false;
    } else {
      uint32_t offset;
      if (!JS::ToUint32(state.cx, values[i], &offset)) return false;
      values[i].setNumber(offset);
    }
  }
  JS::RootedObject prototype(state.cx);
  if (!RangePrototype(state, "StaticRange", &args, &prototype)) return false;
  JS::RootedObject object(state.cx,
      JS_NewObjectWithGivenProto(state.cx, &kStaticRangeClass, prototype));
  if (!object) return false;
  for (unsigned i = 0; i < 4; ++i) JS::SetReservedSlot(object, i, values[i]);
  args.rval().setObject(*object);
  return true;
}
}
