#ifndef RUYIJS_INTERACTION_EVENT_LEGACY_CONVERSION_H
#define RUYIJS_INTERACTION_EVENT_LEGACY_CONVERSION_H
#include "interaction/internal.h"

namespace ruyijs::interaction {
// Per-invocation roots only. Convert every argument before changing the event;
// public ruyijs retains the last ordinary conversion exception, then throws after
// writing converted/default values. Engine OOM/termination stops immediately.
class LegacyConversion {
 public:
  explicit LegacyConversion(JSContext* cx)
      : cx_(cx), exception_(cx), type_(cx) {}
  bool String(JS::HandleValue input, JS::MutableHandleValue out,
              bool empty_on_failure = false);
  bool Int(JS::HandleValue input, int32_t* out);
  bool Float(JS::HandleValue input, float* out);
  bool Uint16(JS::HandleValue input, uint16_t* out);
  bool Type(JS::HandleValue input);
  void Commit(Event* event, const JS::CallArgs& args);
  bool Finish();
 private:
  bool Capture();
  JSContext* cx_;
  JS::RootedValue exception_;
  JS::RootedString type_;
  std::string key_;
  bool failed_ = false;
};
JS::Value LegacyValue(const JS::CallArgs&, unsigned);
JS::Value LegacyTarget(JSContext*, JS::HandleValue);
uint32_t LegacyModifiers(const JS::CallArgs&, unsigned first);
}
#endif
