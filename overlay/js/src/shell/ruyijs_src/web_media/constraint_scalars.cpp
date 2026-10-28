#include "web_media/media_constraints.h"
#include "runtime/web_kit.h"
#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include "js/PropertyAndElement.h"
#include <cmath>
namespace ruyijs::web_media {
namespace {
using Kind = domain::web_media::ConstraintKind;
bool String(JSContext* cx, JS::HandleValue value) {
  JS::RootedString text(cx, JS::ToString(cx, value));
  return !!text;
}
bool StringUnion(JSContext* cx, JS::HandleValue value, bool dictionary) {
  if (value.isObject()) {
    JS::ForOfIterator iterator(cx);
    if (!iterator.init(value, JS::ForOfIterator::AllowNonIterable)) return false;
    if (iterator.valueIsIterable()) {
      JS::RootedValue item(cx); bool done;
      while (true) {
        if (!iterator.next(&item, &done)) return false;
        if (done) return true;
        if (!String(cx, item)) return false;
      }
    }
  }
  if (dictionary && (value.isObject() || value.isNullOrUndefined())) {
    JS::RootedObject object(cx); JS::RootedValue item(cx);
    if (!ConstraintDictionary(cx, value, &object)) return false;
    for (const char* key : {"exact", "ideal"}) {
      if (!ConstraintRead(cx, object, key, &item)) return false;
      if (!item.isUndefined() && !StringUnion(cx, item, false)) return false;
    }
    return true;
  }
  return String(cx, value);
}
bool Scalar(JSContext* cx, JS::HandleValue value, Kind kind) {
  if (kind == Kind::BooleanRange || kind == Kind::Boolean) {
    (void)JS::ToBoolean(value); return true;
  }
  if (kind == Kind::String) return String(cx, value);
  if (kind == Kind::LongRange) {
    int32_t ignored; return JS::ToInt32(cx, value, &ignored);
  }
  if (kind == Kind::LongLong) {
    int64_t ignored; return JS::ToInt64(cx, value, &ignored);
  }
  double number;
  if (!JS::ToNumber(cx, value, &number)) return false;
  return std::isfinite(number) || WebKit::Fail(cx, "Media constraint requires a finite double");
}
}
bool ConvertConstraint(JSContext* cx, JS::HandleValue value, Kind kind) {
  if (kind == Kind::StringRange) return StringUnion(cx, value, true);
  const bool range = kind == Kind::LongRange || kind == Kind::DoubleRange || kind == Kind::BooleanRange;
  if (!range || (!value.isObject() && !value.isNullOrUndefined())) return Scalar(cx, value, kind);
  JS::RootedObject object(cx); JS::RootedValue item(cx);
  if (!ConstraintDictionary(cx, value, &object)) return false;
  for (const char* key : {"exact", "ideal", "max", "min"}) {
    if (kind == Kind::BooleanRange && (key[0] == 'm')) break;
    if (!ConstraintRead(cx, object, key, &item)) return false;
    if (!item.isUndefined() && !Scalar(cx, item, kind)) return false;
  }
  return true;
}
}
