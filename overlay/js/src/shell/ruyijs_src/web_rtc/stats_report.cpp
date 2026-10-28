// RTCStatsReport: a read-only maplike<DOMString, object> whose one state is the
// backing Map in its reserved slot (docs/owners/WEB_RTC.md, 2026-10-21). No
// getStats exists yet, so no report instance is created; the interface,
// members and receiver checks match Firefox.
#include "web_rtc/state.h"

#include <iterator>
#include <string>

#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include "js/MapAndSet.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/ValueArray.h"

namespace ruyijs::web_rtc {
const JSClass kStatsClass = {"RTCStatsReport", JSCLASS_HAS_RESERVED_SLOTS(kStatsSlotCount)};
namespace {
const char* const kStatsName = "RTCStatsReport";
const char* const kStatsMethods[] = {"entries", "keys", "values", "forEach", "has", "get"};
const unsigned kStatsLengths[] = {0, 0, 0, 1, 1, 1};

bool Receiver(const JS::CallArgs& args, const char* member, JS::MutableHandleObject self) {
  JSContext* cx = static_cast<State*>(WebKit::Owner(args))->cx;
  JSObject* object = args.thisv().isObject() ? &args.thisv().toObject() : nullptr;
  if (!object || JS::GetClass(object) != &kStatsClass) {
    const std::string text = std::string("'") + member +
        "' called on an object that does not implement interface RTCStatsReport.";
    return WebKit::Fail(cx, text.c_str());
  }
  self.set(object);
  return true;
}

// Read-only maplike over the report's one backing Map.
bool StatsMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t op = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!Receiver(args, op == kStatsSize ? "get size" : kStatsMethods[op], &self))
    return false;
  JS::RootedObject map(cx, &JS::GetReservedSlot(self, kStatsMap).toObject());
  JS::RootedValue key(cx);
  if ((op == kStatsHas || op == kStatsGet) && args.length()) {
    JS::RootedString text(cx, JS::ToString(cx, args[0]));  // DOMString key conversion
    if (!text) return false;
    key.setString(text);
  }
  switch (op) {
    case kStatsEntries: return JS::MapEntries(cx, map, args.rval());
    case kStatsKeys: return JS::MapKeys(cx, map, args.rval());
    case kStatsValues: return JS::MapValues(cx, map, args.rval());
    case kStatsSize: args.rval().setNumber(double(JS::MapSize(cx, map))); return true;
    case kStatsHas: {
      if (!args.length()) return WebKit::Fail(cx, "RTCStatsReport.has: At least 1 argument required, but only 0 passed");
      bool found = false;
      if (!JS::MapHas(cx, map, key, &found)) return false;
      args.rval().setBoolean(found);
      return true;
    }
    case kStatsGet:
      if (!args.length()) return WebKit::Fail(cx, "RTCStatsReport.get: At least 1 argument required, but only 0 passed");
      return JS::MapGet(cx, map, key, args.rval());
    default: break;
  }
  if (!args.length()) return WebKit::Fail(cx, "RTCStatsReport.forEach: At least 1 argument required, but only 0 passed");
  if (!args[0].isObject() || !JS::IsCallable(&args[0].toObject()))
    return WebKit::Fail(cx, "RTCStatsReport.forEach: Argument 1 is not callable.");
  // The callback's third argument is the report itself, never the backing Map.
  JS::RootedValue pairs(cx), pair(cx), value(cx), callback(cx, args[0]), ignored(cx);
  if (!JS::MapEntries(cx, map, &pairs)) return false;
  JS::ForOfIterator it(cx);
  if (!it.init(pairs)) return false;
  JS::RootedValueArray<3> arguments(cx);
  arguments[2].setObject(*self);
  for (bool done = false;;) {
    if (!it.next(&pair, &done)) return false;
    if (done) break;
    JS::RootedObject entry(cx, &pair.toObject());
    if (!JS_GetElement(cx, entry, 0, &key) || !JS_GetElement(cx, entry, 1, &value)) return false;
    arguments[0].set(value);
    arguments[1].set(key);
    if (!JS::Call(cx, args.get(1), callback, arguments, &ignored)) return false;
  }
  args.rval().setUndefined();
  return true;
}

}  // namespace

bool InstallStatsReport(State& s, JS::HandleObject objectProto) {
  JSContext* cx = s.cx;
  if (!s.kit.Define(kStatsName, &kStatsClass, objectProto, IllegalCreate, &s.stats)) return false;
  for (int32_t i = 0; i < int32_t(std::size(kStatsMethods)); ++i)
    if (!s.kit.Method(s.stats, kStatsName, kStatsMethods[i], StatsMethod, kStatsLengths[i], i))
      return false;
  if (!s.kit.Accessor(s.stats, kStatsName, "size", StatsMethod, nullptr, kStatsSize)) return false;
  JS::RootedValue entries(cx);
  JS::RootedId iterator(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::iterator)));
  return JS_GetProperty(cx, s.stats.prototype, "entries", &entries) &&
         JS_DefinePropertyById(cx, s.stats.prototype, iterator, entries, 0);
}
}  // namespace ruyijs::web_rtc
