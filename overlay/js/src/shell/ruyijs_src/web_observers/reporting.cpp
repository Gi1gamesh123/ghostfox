// ReportingObserver, ReportBody and the internal Report interface. The shape
// follows the Firefox donor 157 (dom/webidl/Reporting.webidl and
// dom/reporting/ReportingObserver.cpp). Report is [LegacyNoInterfaceObject], so
// it stays off the global and is only the prototype a report instance would
// use; ReportBody is exposed and has a throwing default constructor. This
// engine has no report source (no CSP engine, no reporter), so observe() only
// registers and takeRecords() hands out the always-empty ReportList; the D61
// deviation states that no callback can fire.
#include "web_observers/internal.h"
#include "web_observers/realm.h"
#include "document_model/realm.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/StableStringChars.h"
#include "js/String.h"
#include "jsfriendapi.h"
#include <new>

namespace ruyijs::web_observers {
const JSClass kReportingClass = {"ReportingObserver", JSCLASS_HAS_RESERVED_SLOTS(1)};
const JSClass kReportBodyClass = {"ReportBody", JSCLASS_HAS_RESERVED_SLOTS(0)};
const JSClass kReportClass = {"Report", JSCLASS_HAS_RESERVED_SLOTS(3)};

namespace {
Reporting* Receiver(JSContext* cx, JS::CallArgs& args) {
  if (!args.thisv().isObject() || JS::GetClass(&args.thisv().toObject()) != &kReportingClass) {
    auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
    if (state)
      ThrowType(state, 
          "ReportingObserver member called on an incompatible receiver");
    else
      JS_ReportErrorASCII(cx, "ReportingObserver member called on an incompatible receiver");
    return nullptr;
  }
  return static_cast<Reporting*>(JS::GetReservedSlot(&args.thisv().toObject(), 0).toPrivate());
}

// ReportingObserverOptions.types: sequence<DOMString>. A sequence conversion
// requires an object exposing @@iterator; members are stringified.
bool Types(JSContext* cx, State& state, JS::HandleValue value, std::vector<DomString>* types) {
  if (!value.isObject())
    return ThrowType(&state, 
        "ReportingObserver.types is not an iterable sequence");
  JS::ForOfIterator iterate(cx);
  if (!iterate.init(value, JS::ForOfIterator::ThrowOnNonIterable)) return false;
  JS::RootedValue item(cx);
  while (true) {
    bool done = false;
    if (!iterate.next(&item, &done)) return false;
    if (done) break;
    JS::RootedString text(cx, JS::ToString(cx, item));
    if (!text) return false;
    JS::AutoStableStringChars chars(cx);
    if (!chars.initTwoByte(cx, text)) return false;
    if (types->size() >= 4096)
      return ThrowType(&state, "types exceeds the product limit");
    types->emplace_back(chars.twoByteChars(), chars.length());
  }
  return true;
}
}  // namespace

bool ReportingConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  if (!state) { JS_ReportErrorASCII(cx, "Illegal constructor."); return false; }
  if (!args.isConstructing()) return ThrowType(state, "Illegal constructor.");
  if (!args.get(0).isObject() || !JS::IsCallable(&args.get(0).toObject()))
    return ThrowType(state, 
        "ReportingObserver requires a callable reporting callback");
  JS::RootedObject callback(cx, &args.get(0).toObject());
  std::vector<DomString> types;
  bool buffered = false;
  if (!args.get(1).isUndefined()) {
    if (!args.get(1).isObject())
      return ThrowType(state, "ReportingObserver options is not an object");
    JS::RootedObject options(cx, &args.get(1).toObject());
    JS::RootedValue member(cx);
    if (!JS_GetProperty(cx, options, "types", &member)) return false;
    if (!member.isUndefined() && !Types(cx, *state, member, &types)) return false;
    if (!JS_GetProperty(cx, options, "buffered", &member)) return false;
    buffered = !member.isUndefined() && JS::ToBoolean(member);
  }
  JS::RootedObject object(
      cx, JS_NewObjectWithGivenProto(cx, &kReportingClass, state->reportingPrototype));
  if (!object) return false;
  Reporting* observer = state->NewReporting(object, callback);
  if (!observer) return false;
  JS::SetReservedSlot(object, 0, JS::PrivateValue(observer));
  observer->types = std::move(types);
  observer->buffered = buffered;
  args.rval().setObject(*object);
  return state->helpers->Object("ReportingObserver", object) && trace.Finish(true);
}

bool ReportConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  if (!state) { JS_ReportErrorASCII(cx, "Illegal constructor."); return false; }
  return ThrowType(state, "Illegal constructor.");
}

bool ReportBodyConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  if (!state) { JS_ReportErrorASCII(cx, "Illegal constructor."); return false; }
  return ThrowType(state, "Illegal constructor.");
}

bool ReportMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!args.thisv().isObject() || JS::GetClass(&args.thisv().toObject()) != &kReportClass) {
    auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
    if (state)
      return ThrowType(state, "Report member called on an incompatible receiver");
    JS_ReportErrorASCII(cx, "Report member called on an incompatible receiver");
    return false;
  }
  int32_t slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  args.rval().set(JS::GetReservedSlot(&args.thisv().toObject(), size_t(slot)));
  return true;
}

// Report's [Default] toJSON: a fresh object with the report's own members.
bool ReportToJSON(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!args.thisv().isObject() || JS::GetClass(&args.thisv().toObject()) != &kReportClass) {
    auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
    if (state)
      return ThrowType(state, "Report.toJSON called on an incompatible receiver");
    JS_ReportErrorASCII(cx, "Report.toJSON called on an incompatible receiver");
    return false;
  }
  JS::RootedObject self(cx, &args.thisv().toObject());
  JS::RootedObject object(cx, JS_NewPlainObject(cx));
  if (!object) return false;
  static const char* const members[] = {"type", "url", "body"};
  for (int32_t slot = 0; slot < 3; ++slot) {
    JS::RootedValue value(cx, JS::GetReservedSlot(self, size_t(slot)));
    if (!JS_DefineProperty(cx, object, members[slot], value, JSPROP_ENUMERATE)) return false;
  }
  args.rval().setObject(*object);
  return true;
}

bool ReportBodyToJSON(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!args.thisv().isObject() || JS::GetClass(&args.thisv().toObject()) != &kReportBodyClass) {
    auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
    if (state)
      return ThrowType(state, 
          "ReportBody.toJSON called on an incompatible receiver");
    JS_ReportErrorASCII(cx, "ReportBody.toJSON called on an incompatible receiver");
    return false;
  }
  // [Default] toJSON of an interface with no own members: a fresh empty object.
  JS::RootedObject object(cx, JS_NewPlainObject(cx));
  if (!object) return false;
  args.rval().setObject(*object);
  return true;
}

bool ReportingObserve(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Reporting* observer = Receiver(cx, args);
  if (!observer) return false;
  observer->registered = true;
  args.rval().setUndefined();
  return true;
}

bool ReportingDisconnect(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Reporting* observer = Receiver(cx, args);
  if (!observer) return false;
  observer->registered = false;
  args.rval().setUndefined();
  return true;
}

bool ReportingTakeRecords(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Reporting* observer = Receiver(cx, args);
  if (!observer) return false;
  // ReportList; this engine has no report source, so it is always empty.
  JS::RootedObject records(cx, JS::NewArrayObject(cx, 0));
  if (!records) return false;
  args.rval().setObject(*records);
  return true;
}

Reporting* State::NewReporting(JS::HandleObject object, JS::HandleObject callback) {
  if (reportingCount >= kObserverLimit) {
    ThrowType(this, "ReportingObserver limit exceeded");
    return nullptr;
  }
  std::unique_ptr<Reporting> created(new (std::nothrow) Reporting(cx, object, callback));
  if (!created) { JS_ReportOutOfMemory(cx); return nullptr; }
  created->owner = this;
  ++reportingCount;
  Reporting* added = created.get();
  if (!reportings) {
    reportings = std::move(created);
  } else {
    Reporting* tail = reportings.get();
    while (tail->next) tail = tail->next.get();
    tail->next = std::move(created);
  }
  return added;
}

bool InstallReporting(State& s) {
  // ReportBody is [Pref="dom.reporting.enabled"] and exposed (default true).
  JS::RootedObject body_prototype(s.cx), body_constructor(s.cx);
  if (!NewInterface(s, "ReportBody", 0, ReportBodyConstructor, &body_prototype,
                    &body_constructor)) return false;
  if (!Method(s, "ReportBody", body_prototype, "toJSON", ReportBodyToJSON, 0) ||
      !LinkInterface(s, body_prototype, body_constructor) ||
      !JS_DefineProperty(s.cx, s.page, "ReportBody", body_constructor, 0)) return false;
  s.reportBodyPrototype = body_prototype.get();
  // Report is [LegacyNoInterfaceObject]: internal prototype, no global.
  JS::RootedObject report_prototype(s.cx), report_constructor(s.cx);
  if (!NewInterface(s, "Report", 0, ReportConstructor, &report_prototype,
                    &report_constructor)) return false;
  static const char* const report_members[] = {"type", "url", "body"};
  for (int32_t slot = 0; slot < 3; ++slot)
    if (!Getter(s, "Report", report_prototype, report_members[slot], ReportMember, slot))
      return false;
  if (!Method(s, "Report", report_prototype, "toJSON", ReportToJSON, 0) ||
      !LinkInterface(s, report_prototype, report_constructor)) return false;
  s.reportPrototype = report_prototype.get();
  JS::RootedObject prototype(s.cx), constructor(s.cx);
  if (!NewInterface(s, "ReportingObserver", 1, ReportingConstructor, &prototype, &constructor))
    return false;
  static const struct { const char* name; JSNative native; } methods[] = {
      {"observe", ReportingObserve}, {"disconnect", ReportingDisconnect},
      {"takeRecords", ReportingTakeRecords}};
  for (const auto& method : methods)
    if (!Method(s, "ReportingObserver", prototype, method.name, method.native, 0))
      return false;
  if (!LinkInterface(s, prototype, constructor) ||
      !JS_DefineProperty(s.cx, s.page, "ReportingObserver", constructor, 0)) return false;
  s.reportingPrototype = prototype.get();
  return true;
}
}  // namespace ruyijs::web_observers
