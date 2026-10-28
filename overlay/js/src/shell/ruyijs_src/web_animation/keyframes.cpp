// "Process a keyframes argument" (web-animations-1 §5.10.3) and getKeyframes.
// A frame is stored as an internal array [offset|null, easing, composite,
// [[property, value], ...]] the page never sees. Property names are the CSS
// IDL attribute names of the generated property table (cssFloat, not float).
#include "web_animation/state.h"
#include "web_animation/keyframes.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Symbol.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace ruyijs::web_animation {
namespace {
struct DeclarationName { const char* idl; const char* css; const char* attribute; };
constexpr DeclarationName kNames[] = {
#include "document_model/css_properties.inc"
};
const char* const kFrameComposite[] = {"replace", "add", "accumulate", "auto"};
// Firefox converts keyframes inside setKeyframes, so conversion errors carry its
// prefix whichever API received them; only the offset-order error uses the caller's.
const std::string kConvert = "KeyframeEffect.setKeyframes: ";
bool Animatable(const std::string& key) {
  // Not animatable in Firefox (CSS "Animation type: not animatable"), or a keyframe member.
  if (key.rfind("animation", 0) == 0 || key.rfind("transition", 0) == 0 || key == "offset" ||
      key == "display" || key == "direction" || key == "unicodeBidi" || key == "willChange")
    return false;
  for (const auto& n : kNames) if (key == n.idl && key == n.attribute) return true;
  return false;
}
bool Trim(std::string* s) {
  const char* ws = " \t\n\f\r";
  const size_t b = s->find_first_not_of(ws);
  if (b == std::string::npos) return false;
  *s = s->substr(b, s->find_last_not_of(ws) - b + 1);
  return true;
}
bool Iterable(JSContext* cx, JS::HandleValue v, bool* result) {
  *result = false;
  if (!v.isObject()) return true;
  JS::RootedObject object(cx, &v.toObject());
  JS::RootedId id(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::iterator)));
  JS::RootedValue method(cx);
  if (!JS_GetPropertyById(cx, object, id, &method)) return false;
  *result = !method.isNullOrUndefined();
  return true;
}
// A value, or every value of a sequence when lists are allowed.
bool Strings(JSContext* cx, JS::HandleValue v, bool lists, std::vector<std::optional<std::string>>* out) {
  bool iterable = false;
  if (lists && !Iterable(cx, v, &iterable)) return false;
  if (!iterable) {
    std::string text;
    if (v.isUndefined()) { out->push_back(std::nullopt); return true; }
    if (!Text(cx, v, &text)) return false;
    out->push_back(Trim(&text) ? std::optional<std::string>(text) : std::nullopt);
    return true;
  }
  JS::ForOfIterator it(cx);
  JS::RootedValue item(cx);
  if (!it.init(v)) return false;
  for (bool done = false;;) {
    if (!it.next(&item, &done)) return false;
    if (done) return true;
    if (!Strings(cx, item, false, out)) return false;
  }
}
bool Composite(JSContext* cx, JS::HandleValue v, const std::string&, std::string* out) {
  std::string text;
  if (!Text(cx, v, &text)) return false;
  for (const char* name : kFrameComposite) if (text == name) { *out = text; return true; }
  return TypeFail(cx, kConvert + "'" + text + "' (value of 'composite' member of BaseKeyframe) is not a "
                      "valid value for enumeration CompositeOperationOrAuto.");
}
bool Offset(JSContext* cx, JS::HandleValue v, std::optional<double>* out) {
  if (v.isNullOrUndefined()) { out->reset(); return true; }
  double number = 0;
  if (!JS::ToNumber(cx, v, &number)) return false;
  if (!std::isfinite(number))
    return TypeFail(cx, kConvert + "Double branch of ((double or CSSNumericValue) or TimelineRangeOffset or "
                              "USVString) is not a finite floating-point value.");
  *out = number;
  return true;
}
// Own enumerable animatable property names, sorted by code unit.
bool Properties(JSContext* cx, JS::HandleObject object, std::vector<std::string>* names) {
  JS::Rooted<JS::IdVector> ids(cx, JS::IdVector(cx));
  if (!JS_Enumerate(cx, object, &ids)) return false;
  for (size_t i = 0; i < ids.length(); ++i) {
    if (!ids[i].isString()) continue;
    JS::RootedValue key(cx);
    std::string name;
    if (!JS_IdToValue(cx, ids[i], &key) || !Text(cx, key, &name)) return false;
    if (Animatable(name)) names->push_back(name);
  }
  std::sort(names->begin(), names->end());
  return true;
}
bool Easing(JSContext* cx, const std::string& text, std::string* out) {
  model::Easing easing;
  if (!model::ParseEasing(text, &easing)) return TypeFail(cx, "Invalid easing '" + text + "'.");
  *out = easing.text;
  return true;
}
bool ListForm(JSContext* cx, JS::HandleValue input, const std::string& prefix, std::vector<Frame>* frames) {
  JS::ForOfIterator it(cx);
  JS::RootedValue item(cx), v(cx);
  if (!it.init(input)) return false;
  for (bool done = false;;) {
    if (!it.next(&item, &done)) return false;
    if (done) return true;
    if (!item.isNullOrUndefined() && !item.isObject())
      return TypeFail(cx, kConvert + "Element of sequence<Keyframe> argument is not an object.");
    Frame f;
    if (item.isObject()) {
      JS::RootedObject object(cx, &item.toObject());
      std::string easing = "linear";
      std::vector<std::string> names;
      if (!JS_GetProperty(cx, object, "composite", &v) ||
          (!v.isUndefined() && !Composite(cx, v, prefix, &f.composite)) ||
          !JS_GetProperty(cx, object, "easing", &v) || (!v.isUndefined() && !Text(cx, v, &easing)) ||
          !JS_GetProperty(cx, object, "offset", &v) || !Offset(cx, v, &f.offset) ||
          !Properties(cx, object, &names) || !Easing(cx, easing, &f.easing))
        return false;
      for (const auto& name : names) {
        std::vector<std::optional<std::string>> values;
        if (!JS_GetProperty(cx, object, name.c_str(), &v) || !Strings(cx, v, false, &values)) return false;
        if (values[0]) f.values.push_back({name, *values[0]});
      }
    }
    frames->push_back(std::move(f));
  }
}
bool IndexedForm(JSContext* cx, JS::HandleObject object, const std::string& prefix, std::vector<Frame>* frames) {
  JS::RootedValue composite(cx), easing(cx), offset(cx), v(cx);
  std::vector<std::string> names;
  if (!JS_GetProperty(cx, object, "composite", &composite) || !JS_GetProperty(cx, object, "easing", &easing) ||
      !JS_GetProperty(cx, object, "offset", &offset) || !Properties(cx, object, &names)) return false;
  std::map<double, Frame> merged;
  for (const auto& name : names) {
    std::vector<std::optional<std::string>> values;
    if (!JS_GetProperty(cx, object, name.c_str(), &v) || !Strings(cx, v, true, &values)) return false;
    for (size_t i = 0; i < values.size(); ++i) {
      const double at = values.size() == 1 ? 1.0 : double(i) / double(values.size() - 1);
      Frame& f = merged[at];
      if (values[i]) f.values.push_back({name, *values[i]});
    }
  }
  for (auto& entry : merged) frames->push_back(std::move(entry.second));
  bool list = false;
  if (!Iterable(cx, offset, &list)) return false;
  std::vector<std::optional<double>> offsets;
  if (list) {
    JS::ForOfIterator it(cx);
    if (!it.init(offset)) return false;
    for (bool done = false;;) {
      if (!it.next(&v, &done)) return false;
      if (done) break;
      offsets.emplace_back();
      if (!Offset(cx, v, &offsets.back())) return false;
    }
  } else if (!offset.isNullOrUndefined()) {
    offsets.emplace_back();
    if (!Offset(cx, offset, &offsets.back())) return false;
  }
  for (size_t i = 0; i < offsets.size() && i < frames->size(); ++i) (*frames)[i].offset = offsets[i];
  std::vector<std::optional<std::string>> easings, composites;
  if (!easing.isUndefined() && !Strings(cx, easing, true, &easings)) return false;
  if (!composite.isUndefined() && !Strings(cx, composite, true, &composites)) return false;
  for (size_t i = 0; i < frames->size(); ++i) {
    if (!easings.empty() && !Easing(cx, easings[i % easings.size()].value_or(""), &(*frames)[i].easing))
      return false;
    if (!composites.empty()) {
      JS::RootedValue c(cx);
      if (!NewString(cx, composites[i % composites.size()].value_or(""), &c) ||
          !Composite(cx, c, prefix, &(*frames)[i].composite)) return false;
    }
  }
  return true;
}
}  // namespace

bool ReadFrames(JSContext* cx, JS::HandleValue input, const std::string& prefix, std::vector<Frame>* frames) {
  if (!input.isObject()) return true;
  bool list = false;
  JS::RootedObject object(cx, &input.toObject());
  if (!Iterable(cx, input, &list)) return false;
  return list ? ListForm(cx, input, prefix, frames) : IndexedForm(cx, object, prefix, frames);
}
}  // namespace ruyijs::web_animation
