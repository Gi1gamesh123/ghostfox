#include "runtime/api_monitor.h"
#include "domain/observability/api_monitor.h"
#include "runtime/runtime.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"

namespace ruyijs {
namespace observability = ruyijs::domain::observability;

// One Context's monitor state, kept behind the header's opaque declaration.
struct ApiMonitorState {
  observability::ApiMonitorAggregate monitor;
};

namespace {
// Bound on retained records for one Context. The aggregate itself keeps every
// accepted access, so the adapter stops recording instead of growing without
// limit; a drained log accepts records again.
constexpr size_t kMaxRecords = 20000;

const char* TypeName(observability::CallType type) {
  return observability::call_type_name(type);
}
}  // namespace

ApiMonitor::ApiMonitor(JSContext* cx, bool debug)
    : cx_(cx), state_(std::make_unique<ApiMonitorState>()) {
  if (debug) state_->monitor.enable();
}

ApiMonitor::~ApiMonitor() = default;

bool ApiMonitor::enabled() const { return state_->monitor.enabled(); }

void ApiMonitor::Port(void* context, const char* iface, const char* member, int type) {
  static_cast<ApiMonitor*>(context)->Record(iface, member, type);
}

void ApiMonitor::Record(const char* iface, const char* member, int type) {
  // Explicit mapping of the port's documented int contract onto the domain
  // enum, so neither side depends on the other's declaration order.
  observability::CallType kind;
  switch (type) {
    case 0: kind = observability::CallType::PropertyRead; break;
    case 1: kind = observability::CallType::PropertyWrite; break;
    case 2: kind = observability::CallType::MethodCall; break;
    case 3: kind = observability::CallType::Construct; break;
    default: return;
  }
  auto& monitor = state_->monitor;
  if (!monitor.enabled() || monitor.record_count() >= kMaxRecords) return;
  // The installer's own interface and member names; the path is what the source
  // monitor reports, so a watch entry matches the same string.
  std::string path(iface);
  path += ".";
  path += member;
  // Script location of the reading frame, so an entry can be attributed without
  // the adapter holding a second copy of engine state.
  std::string detail;
  JS::AutoFilename filename;
  uint32_t line = 0;
  if (JS::DescribeScriptedCaller(&filename, cx_, &line) && filename.get()) {
    detail = filename.get();
    detail += ":";
    detail += std::to_string(line);
  }
  monitor.record_access(observability::ApiPath(path), kind, detail);
}

bool ApiMonitor::Describe(const std::string& filter, bool drain,
                          JS::MutableHandleValue result) {
  JS::RootedValueVector entries(cx_);
  for (const auto& record : state_->monitor.records()) {
    if (!filter.empty() && record.path().value() != filter) continue;
    JS::RootedObject entry(cx_, JS_NewPlainObject(cx_));
    JS::RootedString path(cx_, JS_NewStringCopyZ(cx_, record.path().value().c_str()));
    JS::RootedString kind(cx_, JS_NewStringCopyZ(cx_, TypeName(record.type())));
    JS::RootedString detail(cx_, JS_NewStringCopyZ(cx_, record.detail().c_str()));
    if (!entry || !path || !kind || !detail ||
        !JS_DefineProperty(cx_, entry, "p", path, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx_, entry, "t", kind, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx_, entry, "d", detail, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx_, entry, "n", double(record.sequence()), JSPROP_ENUMERATE))
      return false;
    if (!entries.append(JS::ObjectValue(*entry))) return false;
  }
  JS::RootedObject array(cx_, JS::NewArrayObject(cx_, entries));
  if (!array) return false;
  if (drain) state_->monitor.clear_records();
  result.setObject(*array);
  return true;
}

bool ApiMonitor::Request(JS::HandleObject request, JS::MutableHandleValue result) {
  std::string action, filter;
  if (!StringProperty(cx_, request, "action", &action)) return false;
  if (action != "peek" && action != "get" && action != "clear") {
    JS_ReportErrorASCII(cx_, "Unknown api_log action");
    return false;
  }
  if (!StringProperty(cx_, request, "filter", &filter, "")) return false;
  if (action == "clear") {
    state_->monitor.clear_records();
    JS::RootedObject object(cx_, JS_NewPlainObject(cx_));
    if (!object || !JS_DefineProperty(cx_, object, "entries", 0.0, JSPROP_ENUMERATE))
      return false;
    result.setObject(*object);
    return true;
  }
  return Describe(filter, action == "get", result);
}
}
