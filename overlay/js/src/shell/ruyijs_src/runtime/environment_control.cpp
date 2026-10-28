#include "runtime/environment_control.h"
#include "runtime/runtime.h"
#include "runtime/arguments.h"
#include "runtime/pipe.h"
#include "browser_environment/environment.h"
#include "jsapi.h"
#include "js/PropertyAndElement.h"

namespace ruyijs {
static const JSErrorFormatString* ProfileErrorFormat(void*, unsigned) {
  static const JSErrorFormatString format{
      "RUYIJS_PROFILE_OBJECT", "environment must be an object", 0, JSEXN_TYPEERR};
  return &format;
}
bool EnvironmentControl::File(const char* filename) {
  std::string data;
  if (!ReadUTF8File(filename, &data)) {
    JS_ReportErrorASCII(cx_, "Failed to read ruyijs environment file");
    return false;
  }
  if (data.size() > Pipe::kMaxFrame) {
    JS_ReportErrorASCII(cx_, "Environment profile exceeds 16 MiB");
    return false;
  }
  JS::RootedObject profile(cx_);
  return ParseRequest(cx_, data, &profile) && owner_.Configure(profile);
}

bool EnvironmentControl::Request(const std::string& operation,
                                 JS::HandleObject request,
                                 JS::MutableHandleValue result) {
  if (operation == "defaults") return owner_.Defaults(result);
  if (frozen_) {
    JS_ReportErrorASCII(cx_, "Environment configuration is frozen after the first page operation");
    return false;
  }
  JS::RootedValue value(cx_);
  if (!JS_GetProperty(cx_, request, "environment", &value)) return false;
  if (!value.isObject()) {
    JS_ReportErrorNumberASCII(cx_, ProfileErrorFormat, nullptr, 0);
    return false;
  }
  JS::RootedObject profile(cx_, &value.toObject());
  return owner_.Configure(profile) && (!configured_ || configured_(configuredContext_)) &&
         owner_.Describe(result);
}
}
