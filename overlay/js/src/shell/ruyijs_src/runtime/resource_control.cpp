#include "runtime/resource_control.h"
#include "runtime/resource_schema.h"
#include "js/Value.h"

namespace ruyijs {
bool ResourceControl::Request(JS::HandleObject request, JS::MutableHandleValue result) {
  using namespace resource_wire;
  using resource_network::Report;
  std::string action;
  if (!Property(cx_, request, "action", &action, 16, TextKind::ASCII)) return false;
  if (action == "status" || action == "clear") {
    if (!Fields(cx_, request, {"id", "op", "action"})) return false;
    if (action == "clear") owner_.Clear();
    return Stats(cx_, owner_.Inspect(), result);
  }
  if (action == "put") {
    resource_network::ResourceInput input;
    resource_network::ResourceStats stats;
    return Input(cx_, request, &input) && Report(cx_, owner_.Put(input, &stats)) && Stats(cx_, stats, result);
  }
  if (action == "get") {
    std::string url;
    resource_network::ResourceSnapshot snapshot;
    bool found = false;
    if (!Fields(cx_, request, {"id", "op", "action", "url"}) ||
        !Property(cx_, request, "url", &url, resource_network::Limits::key_bytes) ||
        !Report(cx_, owner_.Get(url, &snapshot, &found))) return false;
    if (found) return Snapshot(cx_, snapshot, result);
    result.setNull();
    return true;
  }
  if (action == "log") {
    resource_network::NetLogRecords entries;
    if (!Fields(cx_, request, {"id", "op", "action"}) ||
        !Report(cx_, owner_.Log(&entries))) return false;
    return LogEntries(cx_, entries, result);
  }
  return Error(cx_, "Unknown resource action");
}
}
