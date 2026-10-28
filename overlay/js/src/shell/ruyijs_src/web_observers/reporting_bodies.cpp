// CSPViolationReportBody and IntegrityViolationReportBody (Firefox 158
// Reporting.webidl, Exposed=Window): ReportBody subclasses a ReportingObserver
// would deliver. This engine has no report source (D61), so neither has
// instances: the interface objects exist with illegal constructors and every
// member fails its WebIDL brand check with Firefox's text.
#include "web_observers/internal.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/PropertyAndElement.h"
#include <iterator>
#include <string>

namespace ruyijs::web_observers {
namespace {
struct Body { const char* name; const char* members[11]; };
const Body kBodies[] = {
    {"CSPViolationReportBody", {"documentURL", "referrer", "blockedURL", "effectiveDirective", "originalPolicy",
                                "sourceFile", "sample", "disposition", "statusCode", "lineNumber", "columnNumber"}},
    {"IntegrityViolationReportBody", {"documentURL", "blockedURL", "destination", "reportOnly"}},
};
State* Owner(const JS::CallArgs& args) {
  return static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
}
bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) {
  return ThrowType(Owner(JS::CallArgsFromVp(argc, vp)), "Illegal constructor.");
}
// Slot: body index * 100 + member (-1 for toJSON).
bool Brand(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  const Body& body = kBodies[slot / 100];
  const int member = slot % 100;
  const std::string label = member == 99 ? std::string("toJSON") : std::string("get ") + body.members[member];
  return ThrowType(Owner(args), ("'" + label + "' called on an object that does not implement interface " +
                                 body.name + ".").c_str());
}
}  // namespace

bool InstallReportBodies(State& s) {
  JSContext* cx = s.cx;
  JS::RootedObject parentCtor(cx);
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, s.page, "ReportBody", &value) || !value.isObject()) return false;
  parentCtor = &value.toObject();
  JS::RootedObject parentProto(cx, s.reportBodyPrototype);
  for (int index = 0; index < int(std::size(kBodies)); ++index) {
    const Body& body = kBodies[index];
    JS::RootedObject prototype(cx), constructor(cx);
    if (!NewInterface(s, body.name, 0, Illegal, &prototype, &constructor) ||
        !JS_SetPrototype(cx, prototype, parentProto) || !JS_SetPrototype(cx, constructor, parentCtor)) return false;
    JS::RootedObject toJSON(cx, Function(s, Brand, "toJSON", 0, index * 100 + 99));
    if (!toJSON || !JS_DefineProperty(cx, prototype, "toJSON", toJSON, JSPROP_ENUMERATE)) return false;
    for (int member = 0; member < 11 && body.members[member]; ++member)
      if (!Getter(s, body.name, prototype, body.members[member], Brand, index * 100 + member)) return false;
    if (!LinkInterface(s, prototype, constructor) || !JS_DefineProperty(cx, s.page, body.name, constructor, 0))
      return false;
  }
  return true;
}
}  // namespace ruyijs::web_observers
