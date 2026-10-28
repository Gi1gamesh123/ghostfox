#include "document_model/range.h"
#include "document_model/state.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto field = static_cast<RangeField>(FunctionSlot(args));
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!object) return TypeError(cx, "Range receiver is incompatible");
  JS::RootedValue result(cx);
  if (JS::GetClass(object) == &kStaticRangeClass) {
    JSAutoRealm realm(cx, object);
    if (field == RangeField::Collapsed) {
      result.setBoolean(JS::GetReservedSlot(object, 0) == JS::GetReservedSlot(object, 2) &&
                        JS::GetReservedSlot(object, 1) == JS::GetReservedSlot(object, 3));
    } else {
      result = JS::GetReservedSlot(object, unsigned(field));
    }
  } else if (JS::GetClass(object) == &kRangeClass) {
    auto* range = static_cast<Range*>(JS::GetReservedSlot(object, 0).toPrivate());
    if (!range || !range->owner) return TypeError(cx, "Range receiver is incompatible");
    State& owner = RangeTree(*range);
    JSAutoRealm realm(cx, owner.page);
    switch (field) {
      case RangeField::StartContainer:
        if (!owner.Wrap(range->start.node, &result)) return false;
        break;
      case RangeField::EndContainer:
        if (!owner.Wrap(range->end.node, &result)) return false;
        break;
      case RangeField::StartOffset: result.setNumber(range->start.offset); break;
      case RangeField::EndOffset: result.setNumber(range->end.offset); break;
      case RangeField::Collapsed:
        result.setBoolean(Same(range->start.node, range->end.node) &&
                          range->start.offset == range->end.offset);
        break;
      default: return false;
    }
  } else return TypeError(cx, "Range receiver is incompatible");
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result);
  return true;
}
}

bool IsAbstractRangeValue(JS::HandleValue value) {
  JSObject* object = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
  return object && (JS::GetClass(object) == &kRangeClass || JS::GetClass(object) == &kStaticRangeClass);
}
bool InstallAbstractRange(State& state) {
  const char* names[] = {"startContainer", "startOffset", "endContainer", "endOffset", "collapsed"};
  for (int32_t i = 0; i < 5; ++i)
    if (!state.Accessor("AbstractRange", names[i], Get, i)) return false;
  return true;
}
}
