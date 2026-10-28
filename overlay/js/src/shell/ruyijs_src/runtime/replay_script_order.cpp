#include "runtime/replay_script_order.h"
#include "runtime/resource_schema.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"

namespace ruyijs {
bool ReplayScriptOrder::Contains(const std::string& url) const {
  for (const auto& value : urls) if (value == url) return true;
  return false;
}
bool ReadReplayScriptOrder(JSContext* cx, JS::HandleObject request, ReplayScriptOrder* order) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, request, "replay_async_scripts", &value)) return false;
  if (value.isUndefined()) return true;
  bool array = false;
  if (!JS::IsArrayObject(cx, value, &array)) return false;
  if (!array) return resource_wire::Error(cx, "replay_async_scripts must be an array");
  JS::RootedObject items(cx, &value.toObject());
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, items, &length)) return false;
  if (length > 64) return resource_wire::Error(cx, "replay script order exceeds 64 entries", true);
  for (uint32_t index = 0; index < length; ++index) {
    std::string url;
    if (!JS_GetElement(cx, items, index, &value) ||
        !resource_wire::Text(cx, value, &url, 8192)) return false;
    if (url.empty() || url.find(':') == std::string::npos || url.find('\0') != std::string::npos || order->Contains(url))
      return resource_wire::Error(cx, "replay script URLs must be absolute, nonempty and unique");
    order->urls.push_back(std::move(url));
  }
  return true;
}
}
