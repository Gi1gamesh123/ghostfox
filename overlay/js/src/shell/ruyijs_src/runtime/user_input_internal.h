#ifndef RUYIJS_RUNTIME_USER_INPUT_INTERNAL_H
#define RUYIJS_RUNTIME_USER_INPUT_INTERNAL_H
// Shared by user_input.cpp and user_keyboard.cpp (ARCHITECTURE.md, host user input).
#include "runtime/scheduling_control.h"
#include "interaction/realm.h"
#include "jsapi.h"
#include "js/PropertyAndElement.h"
#include <string>

namespace ruyijs::user_input {
// One event's init dictionary, dispatched as a trusted event at the clock's now.
class Init {
 public:
  explicit Init(JSContext* cx) : cx_(cx), object_(cx, JS_NewPlainObject(cx)) { ok_ = object_; }
  Init& Number(const char* name, double value) {
    JS::RootedValue v(cx_, JS::NumberValue(value));
    ok_ = ok_ && JS_DefineProperty(cx_, object_, name, v, JSPROP_ENUMERATE);
    return *this;
  }
  Init& Flag(const char* name, bool value) {
    JS::RootedValue v(cx_, JS::BooleanValue(value));
    ok_ = ok_ && JS_DefineProperty(cx_, object_, name, v, JSPROP_ENUMERATE);
    return *this;
  }
  Init& Text(const char* name, const std::u16string& value) {
    JS::RootedString text(cx_, JS_NewUCStringCopyN(cx_, value.data(), value.size()));
    JS::RootedValue v(cx_, text ? JS::StringValue(text) : JS::UndefinedValue());
    ok_ = ok_ && text && JS_DefineProperty(cx_, object_, name, v, JSPROP_ENUMERATE);
    return *this;
  }
  bool Send(JS::HandleObject target, const char* type, SchedulingControl& clock, bool* allowed = nullptr) {
    bool ignored = true;
    return ok_ && interaction::DispatchTrusted(cx_, target, type, object_, clock.ClockMilliseconds(),
                                               allowed ? allowed : &ignored);
  }
 private:
  JSContext* cx_;
  JS::RootedObject object_;
  bool ok_ = false;
};
// One character typed into `target`, and one named key pressed on it.
bool Character(JSContext* cx, JS::HandleObject target, char16_t c, SchedulingControl& clock);
bool Press(JSContext* cx, JS::HandleObject target, const std::string& name, SchedulingControl& clock);
}
#endif
