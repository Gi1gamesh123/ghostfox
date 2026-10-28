#include "document_model/range.h"

#include <algorithm>
#include <new>

#include "document_model/state.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "runtime/web_kit.h"
#include <string>

namespace ruyijs::document_model {
const JSClass kRangeClass = {"Range", JSCLASS_HAS_RESERVED_SLOTS(1), nullptr};

Range* RangeOf(State& state, const JS::CallArgs& args) {
  if (!args.thisv().isObject() ||
      JS::GetClass(&args.thisv().toObject()) != &kRangeClass) {
    // The WebIDL brand message Firefox reports ("'m' called on an object ...").
    std::string text;
    if (WebKit::BrandText(state.cx, args, "Range", &text)) TypeError(state.cx, text.c_str());
    return nullptr;
  }
  auto* range = static_cast<Range*>(
      JS::GetReservedSlot(&args.thisv().toObject(), 0).toPrivate());
  if (!range || range->owner != &state) {
    TypeError(state.cx, "Range receiver is incompatible with this realm");
    return nullptr;
  }
  return range;
}

void RangeRehome(Range& range, State& target, const BoundaryPoint& point) {
  State* next = &target == range.owner ? nullptr : &target;
  if (next != range.tree) {
    if (range.tree) std::erase(range.tree->guests, &range);
    if (next) next->guests.push_back(&range);
    range.tree = next;
  }
  range.start = range.end = point;
}

void RangeRetire(State& state) noexcept {
  for (Range* range = state.ranges.get(); range; range = range->next.get())
    if (range->tree) { std::erase(range->tree->guests, range); range->tree = nullptr; }
  for (Range* guest : state.guests) {
    guest->tree = nullptr;
    guest->start = guest->end = BoundaryPoint{guest->document, 0};
  }
  state.guests.clear();
}

bool RangePrototype(State& state, const char* name, const JS::CallArgs* args,
                    JS::MutableHandleObject prototype) {
  Interface* iface = state.Find(name);
  if (!iface) {
    TypeError(state.cx, "Range interface is not installed");
    return false;
  }
  prototype.set(iface->prototype);
  if (args) {
    JS::RootedObject target(state.cx, &args->newTarget().toObject());
    JS::RootedValue value(state.cx);
    if (!JS_GetProperty(state.cx, target, "prototype", &value)) return false;
    if (value.isObject()) prototype.set(&value.toObject());
  }
  return true;
}

Range* NewRange(State& state, JS::MutableHandleValue result, const JS::CallArgs* args) {
  JS::RootedObject prototype(state.cx);
  if (!RangePrototype(state, "Range", args, &prototype)) return nullptr;
  JS::RootedObject object(state.cx,
      JS_NewObjectWithGivenProto(state.cx, &kRangeClass, prototype));
  std::unique_ptr<Range> entry(new (std::nothrow) Range(state.cx));
  if (!object || !entry) {
    JS_ReportOutOfMemory(state.cx);
    return nullptr;
  }
  entry->owner = &state;
  entry->object = object;
  JS::SetReservedSlot(object, 0, JS::PrivateValue(entry.get()));
  Range* raw = entry.get();
  entry->next = std::move(state.ranges);
  state.ranges = std::move(entry);
  result.setObject(*object);
  return raw;
}

bool CreateRange(State& state, NodeKey document, JS::MutableHandleValue result,
                 const JS::CallArgs* args) {
  Range* range = NewRange(state, result, args);
  if (!range) return false;
  range->document = document;
  range->start = range->end = BoundaryPoint{document, 0};
  return true;
}
}  // namespace ruyijs::document_model
