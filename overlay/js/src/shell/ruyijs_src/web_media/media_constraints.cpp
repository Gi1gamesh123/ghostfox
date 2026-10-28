#include "web_media/media_constraints.h"
#include "runtime/web_kit.h"
#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include "js/PropertyAndElement.h"
namespace ruyijs::web_media {
bool ConstraintDictionary(JSContext* cx, JS::HandleValue value, JS::MutableHandleObject out) {
  if (value.isNullOrUndefined()) { out.set(nullptr); return true; }
  if (!value.isObject()) return WebKit::Fail(cx, "Media options must be a dictionary");
  out.set(&value.toObject()); return true;
}
bool ConstraintRead(JSContext* cx, JS::HandleObject object, const char* key, JS::MutableHandleValue out) {
  if (!object) { out.setUndefined(); return true; }
  return JS_GetProperty(cx, object, key, out);
}
namespace {
bool TrackConstraints(JSContext* cx, JS::HandleValue value, bool advanced) {
  JS::RootedObject object(cx); JS::RootedValue item(cx);
  if (!ConstraintDictionary(cx, value, &object)) return false;
  for (const auto& field : domain::web_media::kConstraintFields) {
    if (!ConstraintRead(cx, object, field.name, &item)) return false;
    if (!item.isUndefined() && !ConvertConstraint(cx, item, field.kind)) return false;
  }
  if (!advanced) return true;
  if (!ConstraintRead(cx, object, "advanced", &item)) return false;
  if (item.isUndefined()) return true;
  if (!item.isObject()) return WebKit::Fail(cx, "Media constraints advanced must be a sequence");
  JS::ForOfIterator iterator(cx);
  if (!iterator.init(item, JS::ForOfIterator::ThrowOnNonIterable)) return false;
  bool done;
  while (true) {
    if (!iterator.next(&item, &done)) return false;
    if (done) return true;
    if (!TrackConstraints(cx, item, false)) return false;
  }
}
}
bool ReadDisplayConstraints(JSContext* cx, JS::HandleValue value) {
  JS::RootedObject object(cx); JS::RootedValue item(cx);
  if (!ConstraintDictionary(cx, value, &object)) return false;
  for (const char* key : {"audio", "video"}) {
    if (!ConstraintRead(cx, object, key, &item)) return false;
    if (item.isObject() || item.isNullOrUndefined()) {
      if (!TrackConstraints(cx, item, true)) return false;
    } else {
      (void)JS::ToBoolean(item);
    }
  }
  return true;
}
bool ReadOutputOptions(JSContext* cx, JS::HandleValue value) {
  JS::RootedObject object(cx); JS::RootedValue item(cx);
  if (!ConstraintDictionary(cx, value, &object) ||
      !ConstraintRead(cx, object, "deviceId", &item)) return false;
  if (item.isUndefined()) return true;
  JS::RootedString text(cx, JS::ToString(cx, item));
  return !!text;
}
}
