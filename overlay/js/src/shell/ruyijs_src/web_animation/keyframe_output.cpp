// Keyframe storage and getKeyframes (web-animations-1 §5.10). A stored frame is
// an internal array [offset|null, easing, composite, [[property, value], ...]]
// created with define (no page accessor runs) and never handed to the page.
#include "web_animation/state.h"
#include "web_animation/keyframes.h"
#include "js/Array.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include <vector>

namespace ruyijs::web_animation {
namespace {
bool Element(JSContext* cx, JS::HandleObject array, uint32_t index, JS::MutableHandleValue out) {
  return JS_GetElement(cx, array, index, out);
}
}  // namespace

bool ParseKeyframes(JSContext* cx, JS::HandleValue input, const std::string& prefix,
                    JS::MutableHandleObject result) {
  std::vector<Frame> frames;
  if (!ReadFrames(cx, input, prefix, &frames)) return false;
  double previous = 0;
  for (const Frame& f : frames) {
    if (!f.offset) continue;
    if (*f.offset < previous || *f.offset > 1)
      return TypeFail(cx, prefix + "Keyframes with specified offsets must be in order and all be "
                                   "in the range [0, 1].");
    previous = *f.offset;
  }
  result.set(JS::NewArrayObject(cx, 0));
  if (!result) return false;
  JS::RootedValue v(cx);
  for (size_t i = 0; i < frames.size(); ++i) {
    const Frame& f = frames[i];
    JS::RootedObject frame(cx, JS::NewArrayObject(cx, 0)), values(cx, JS::NewArrayObject(cx, 0));
    if (!frame || !values) return false;
    for (size_t j = 0; j < f.values.size(); ++j) {
      JS::RootedObject pair(cx, JS::NewArrayObject(cx, 0));
      JS::RootedValue name(cx), value(cx);
      if (!pair || !NewString(cx, f.values[j].first, &name) || !NewString(cx, f.values[j].second, &value) ||
          !JS_DefineElement(cx, pair, 0, name, 0) || !JS_DefineElement(cx, pair, 1, value, 0) ||
          !JS_DefineElement(cx, values, uint32_t(j), pair, 0)) return false;
    }
    if (f.offset) v.setDouble(*f.offset); else v.setNull();
    if (!JS_DefineElement(cx, frame, 0, v, 0) || !NewString(cx, f.easing, &v) ||
        !JS_DefineElement(cx, frame, 1, v, 0) || !NewString(cx, f.composite, &v) ||
        !JS_DefineElement(cx, frame, 2, v, 0) || !JS_DefineElement(cx, frame, 3, values, 0) ||
        !JS_DefineElement(cx, result, uint32_t(i), frame, 0)) return false;
  }
  return true;
}

// getKeyframes(): fresh objects, offsets computed by "compute missing keyframe offsets".
bool KeyframesObject(JSContext* cx, JS::HandleObject frames, JS::MutableHandleValue result) {
  uint32_t count = 0;
  if (!JS::GetArrayLength(cx, frames, &count)) return false;
  std::vector<std::optional<double>> computed(count);
  JS::RootedValue v(cx), frameValue(cx);
  for (uint32_t i = 0; i < count; ++i) {
    if (!Element(cx, frames, i, &frameValue)) return false;
    JS::RootedObject frame(cx, &frameValue.toObject());
    if (!Element(cx, frame, 0, &v)) return false;
    if (v.isNumber()) computed[i] = v.toNumber();
  }
  if (count > 1 && !computed.front()) computed.front() = 0.0;
  if (count && !computed.back()) computed.back() = 1.0;
  for (uint32_t i = 1; i < count;) {
    if (computed[i]) { ++i; continue; }
    uint32_t j = i;
    while (!computed[j]) ++j;
    const double from = *computed[i - 1], to = *computed[j];
    for (uint32_t k = i; k < j; ++k) computed[k] = from + (to - from) * double(k - i + 1) / double(j - i + 1);
    i = j;
  }
  JS::RootedObject array(cx, JS::NewArrayObject(cx, 0));
  if (!array) return false;
  for (uint32_t i = 0; i < count; ++i) {
    if (!Element(cx, frames, i, &frameValue)) return false;
    JS::RootedObject frame(cx, &frameValue.toObject()), out(cx, JS_NewPlainObject(cx));
    JS::RootedValue offset(cx), easing(cx), composite(cx), values(cx), pair(cx), name(cx), value(cx);
    if (!out || !Element(cx, frame, 0, &offset) || !Element(cx, frame, 1, &easing) ||
        !Element(cx, frame, 2, &composite) || !Element(cx, frame, 3, &values) ||
        !JS_DefineProperty(cx, out, "composite", composite, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, out, "easing", easing, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, out, "offset", offset, JSPROP_ENUMERATE)) return false;
    v.setNumber(*computed[i]);
    if (!JS_DefineProperty(cx, out, "computedOffset", v, JSPROP_ENUMERATE)) return false;
    JS::RootedObject list(cx, &values.toObject());
    uint32_t properties = 0;
    if (!JS::GetArrayLength(cx, list, &properties)) return false;
    for (uint32_t j = 0; j < properties; ++j) {
      if (!Element(cx, list, j, &pair)) return false;
      JS::RootedObject entry(cx, &pair.toObject());
      std::string key;
      if (!Element(cx, entry, 0, &name) || !Element(cx, entry, 1, &value) || !Text(cx, name, &key) ||
          !JS_DefineProperty(cx, out, key.c_str(), value, JSPROP_ENUMERATE)) return false;
    }
    JS::RootedValue item(cx, JS::ObjectValue(*out));
    if (!JS_DefineElement(cx, array, i, item, JSPROP_ENUMERATE)) return false;
  }
  result.setObject(*array);
  return true;
}
}  // namespace ruyijs::web_animation
