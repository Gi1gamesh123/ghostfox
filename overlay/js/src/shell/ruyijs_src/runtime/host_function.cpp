#include "runtime/host_bridge.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/PropertyAndElement.h"

namespace ruyijs {
bool HostBridge::MakeCallback(JS::HandleString key, JS::HandleString name,
                               JS::MutableHandleObject result) {
  if (functions_.get().length() >= 100000) {
    JS_ReportErrorASCII(cx_, "Host function installation limit exceeded");
    return false;
  }
  JS::RootedId id(cx_);
  if (!JS_StringToId(cx_, name, &id)) return false;
  JS::RootedFunction function(cx_, id.isInt()
      ? js::NewFunctionWithReserved(cx_, Native, 0, 0, nullptr)
      : js::NewFunctionByIdWithReserved(cx_, Native, 0, 0, id));
  if (!function) return false;
  JS::RootedObject object(cx_, JS_GetFunctionObject(function));
  if (!JS_DefineProperty(cx_, object, "name", name, JSPROP_READONLY)) return false;
  js::SetFunctionNativeReserved(object, 0, JS::PrivateValue(this));
  js::SetFunctionNativeReserved(object, 1, JS::StringValue(key));
  if (!functions_.get().append(object)) {
    JS_ReportOutOfMemory(cx_);
    return false;
  }
  if (!RegisterFunction(object, name)) return false;
  result.set(object);
  return true;
}
}
