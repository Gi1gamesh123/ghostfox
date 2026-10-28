// RTCError (docs/owners/WEB_RTC.md, 2026-10-21): an interface extending
// DOMException. Every instance is the DOMException owner's one native storage
// (name "OperationError", code 0), created and read through the exception
// ports the runtime binds; this owner keeps only its RTCErrorInit fields, as an
// internal array in the instance's subclass value. No page method is called.
#include "web_rtc/state.h"

#include <cmath>
#include <string>

#include "js/Array.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"

namespace ruyijs::web_rtc {
namespace {
// RTCErrorInit members in WebIDL dictionary (lexicographic) order; the
// subclass value stores them at these indices.
enum Field : uint32_t { kDetail = 0, kReceived, kCause, kLine, kSent, kFieldCount };
struct Member { const char* name; Field field; bool isUnsigned; };
const Member kInit[] = {{"errorDetail", kDetail, false}, {"receivedAlert", kReceived, true},
                        {"sctpCauseCode", kCause, false}, {"sdpLineNumber", kLine, false},
                        {"sentAlert", kSent, true}};
// Firefox's prototype order of the five read-only attributes.
const Member kProps[] = {{"errorDetail", kDetail, false}, {"sdpLineNumber", kLine, false},
                         {"sctpCauseCode", kCause, false}, {"receivedAlert", kReceived, true},
                         {"sentAlert", kSent, true}};
const char* const kDetails[] = {"data-channel-failure", "dtls-failure", "fingerprint-failure",
                                "sctp-failure", "sdp-syntax-error",
                                "hardware-encoder-not-available", "hardware-encoder-error"};

bool Fail(JSContext* cx, const std::string& text) { return WebKit::Fail(cx, text.c_str()); }

bool Detail(JSContext* cx, JS::HandleValue value, JS::MutableHandleValue out) {
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text) return false;
  for (const char* name : kDetails) {
    bool same = false;
    if (!JS_StringEqualsAscii(cx, text, name, &same)) return false;
    if (same) { out.setString(text); return true; }
  }
  JS::UniqueChars bytes = JS_EncodeStringToUTF8(cx, text);
  if (!bytes) return false;
  return Fail(cx, std::string("RTCError constructor: '") + bytes.get() +
                  "' (value of 'errorDetail' member of RTCErrorInit) is not a valid value for "
                  "enumeration RTCErrorDetailType.");
}

// new RTCError(RTCErrorInit init, optional DOMString message = "")
bool ErrorCreate(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  if (!args.isConstructing()) return Fail(cx, "RTCError constructor: 'new' is required");
  if (!args.length())
    return Fail(cx, "RTCError constructor: At least 1 argument required, but only 0 passed");
  JS::HandleValue init = args[0];
  if (!init.isNullOrUndefined() && !init.isObject())
    return Fail(cx, "RTCError constructor: Argument 1 can't be converted to a dictionary.");
  JS::RootedObject fields(cx, JS::NewArrayObject(cx, kFieldCount));
  if (!fields) return false;
  JS::RootedValue value(cx), stored(cx);
  JS::RootedObject dictionary(cx, init.isObject() ? &init.toObject() : nullptr);
  for (const Member& m : kInit) {
    value.setUndefined();
    if (dictionary && !JS_GetProperty(cx, dictionary, m.name, &value)) return false;
    stored.setNull();
    if (m.field == kDetail) {
      if (value.isUndefined())
        return Fail(cx, "RTCError constructor: Missing required 'errorDetail' member of RTCErrorInit.");
      if (!Detail(cx, value, &stored)) return false;
    } else if (!value.isUndefined()) {
      if (m.isUnsigned) {
        uint32_t number = 0;
        if (!JS::ToUint32(cx, value, &number)) return false;
        stored.setNumber(double(number));
      } else {
        int32_t number = 0;
        if (!JS::ToInt32(cx, value, &number)) return false;
        stored.setInt32(number);
      }
    }
    // Defined, not assigned: no Array.prototype accessor of the page can run.
    if (!JS_DefineElement(cx, fields, m.field, stored, JSPROP_ENUMERATE)) return false;
  }
  JS::RootedString message(cx, JS_GetEmptyString(cx));
  if (!args.get(1).isUndefined() && !(message = JS::ToString(cx, args[1]))) return false;
  JS::RootedObject proto(cx);
  if (!s.kit.ConstructProto(args, s.error.prototype, &proto)) return false;
  JS::RootedValue data(cx, JS::ObjectValue(*fields));
  return s.exceptions.create(cx, proto, message, "OperationError", data, args.rval());
}

bool ErrorGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  const auto field = uint32_t(WebKit::Slot(args));
  JS::RootedValue data(cx);
  JSObject* self = args.thisv().isObject() ? &args.thisv().toObject() : nullptr;
  if (!self || !s.exceptions.data(self, &data) || !data.isObject()) {
    const char* name = "";
    for (const Member& m : kProps) if (m.field == field) name = m.name;
    return Fail(cx, std::string("'get ") + name +
                    "' called on an object that does not implement interface RTCError.");
  }
  JS::RootedObject fields(cx, &data.toObject());
  return JS_GetElement(cx, fields, field, args.rval());
}
}  // namespace

bool InstallError(State& s) {
  JSContext* cx = s.cx;
  if (!s.exceptions.create || !s.exceptions.data) return true;  // Unbound: not exposed.
  // Install time, before any page script: the realm's own DOMException
  // interface prototype object is the parent of RTCError.prototype.
  JS::RootedValue ctor(cx), proto(cx);
  if (!JS_GetProperty(cx, s.kit.page, "DOMException", &ctor) || !ctor.isObject()) return false;
  JS::RootedObject domCtor(cx, &ctor.toObject());
  if (!JS_GetProperty(cx, domCtor, "prototype", &proto) || !proto.isObject()) return false;
  JS::RootedObject parent(cx, &proto.toObject());
  if (!s.kit.Define("RTCError", nullptr, parent, ErrorCreate, &s.error, 1)) return false;
  for (const Member& m : kProps)
    if (!s.kit.Accessor(s.error, "RTCError", m.name, ErrorGet, nullptr, int32_t(m.field)))
      return false;
  return true;
}
}  // namespace ruyijs::web_rtc
