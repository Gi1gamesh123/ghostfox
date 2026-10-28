// Host registrations that survive a top-level navigation
// (docs/owners/TOP_LEVEL.md). A new document gets a new global, so the bridge
// installs the recorded expose/native/expose_data bindings into it again,
// through the same three entries, before any of its scripts run.
#include "runtime/host_bridge.h"
#include "jsapi.h"
#include "js/String.h"
#include <new>

namespace ruyijs {
bool HostBridge::Record(Kind kind, JS::HandleString name, JS::HandleString code,
                        bool constructor, JS::HandleValue value) {
  if (replaying_) return true;
  const bool data = kind == Kind::Data;
  for (auto it = registrations_.begin(); it != registrations_.end(); ++it) {
    if (((*it)->kind == Kind::Data) != data) continue;
    int32_t order = 0;
    if (!JS_CompareStrings(cx_, (*it)->name, name, &order)) return false;
    if (order) continue;
    // A data entry keeps its place in the dictionary; a global is redefined.
    if (data) { (*it)->value = value; return true; }
    registrations_.erase(it);
    break;
  }
  std::unique_ptr<Registration> entry(new (std::nothrow) Registration(cx_));
  if (!entry) { JS_ReportOutOfMemory(cx_); return false; }
  entry->kind = kind;
  entry->name = name;
  entry->code = code;
  entry->constructor = constructor;
  entry->value = value;
  registrations_.push_back(std::move(entry));
  return true;
}
bool HostBridge::Activate(JS::HandleObject page, PageHelpers& helpers) {
  page_ = page;
  helpers_ = &helpers;
  data_ = nullptr;
  replaying_ = true;
  bool ok = true;
  for (const auto& entry : registrations_) {
    JS::RootedString name(cx_, entry->name), code(cx_, entry->code);
    JS::RootedValue value(cx_, entry->value);
    ok = entry->kind == Kind::Function ? Expose(name)
       : entry->kind == Kind::Native ? NativeWrap(name, code, entry->constructor)
       : ExposeData(name, value);
    if (!ok) break;
  }
  replaying_ = false;
  return ok;
}
}
