#include "runtime/host_bridge.h"
#include "runtime/pipe.h"
#include "runtime/typed_decode.h"
#include "runtime/exception_capture.h"
#include "jsapi.h"
#include "js/CharacterEncoding.h"
#include "js/ErrorReport.h"
#include "js/Exception.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Stack.h"
#include "js/String.h"
#include "js/Wrapper.h"

namespace ruyijs {
static JSExnType HostErrorType(const std::string& name) {
  if (name == "TypeError") return JSEXN_TYPEERR;
  if (name == "RangeError") return JSEXN_RANGEERR;
  if (name == "ReferenceError") return JSEXN_REFERENCEERR;
  if (name == "SyntaxError") return JSEXN_SYNTAXERR;
  if (name == "URIError") return JSEXN_URIERR;
  if (name == "EvalError") return JSEXN_EVALERR;
  return JSEXN_ERR;
}

static bool ThrowHostError(JSContext* cx, const std::string& name,
                            const std::string& message,
                            const std::string& remoteStack) {
  JS::RootedObject stack(cx);
  if (!JS::CaptureCurrentStack(cx, &stack, JS::StackCapture(JS::MaxFrames(32))))
    return false;
  JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx,
      JS::UTF8Chars(message.data(), message.size())));
  JS::RootedString filename(cx, JS_NewStringCopyZ(cx, "ruyijs:callback"));
  JS::Rooted<mozilla::Maybe<JS::Value>> cause(cx, mozilla::Nothing());
  JS::RootedValue error(cx);
  if (!text || !filename || !JS::CreateError(cx,
      HostErrorType(name), stack, filename, 1,
      JS::ColumnNumberOneOrigin(), nullptr, text, cause, &error)) return false;
  JS::RootedObject object(cx, &error.toObject());
  JS::RootedString remote(cx, JS_NewStringCopyUTF8N(cx,
      JS::UTF8Chars(remoteStack.data(), remoteStack.size())));
  JS::RootedString hostName(cx, JS_NewStringCopyUTF8N(cx,
      JS::UTF8Chars(name.data(), name.size())));
  if (!remote || !hostName ||
      !JS_DefineProperty(cx, object, "hostStack", remote, JSPROP_READONLY) ||
      !JS_DefineProperty(cx, object, "hostName", hostName, JSPROP_READONLY)) return false;
  JS_SetPendingException(cx, error);
  return false;
}

bool HostBridge::PageException() {
  if (!JS_IsExceptionPending(cx_)) return false;
  JS::ExceptionStack exception(cx_);
  if (!JS::StealPendingExceptionStack(cx_, &exception)) return false;
  if (!exception.exception().isObject() ||
      !js::IsCrossCompartmentWrapper(&exception.exception().toObject())) {
    JS::SetPendingExceptionStack(cx_, exception);
    return false;
  }
  ExceptionReport report = DescribeException(cx_, exception.exception(), exception.stack());
  return ThrowHostError(cx_, report.name, report.message, report.stack);
}

bool HostBridge::ReadReply(uint64_t callId, JS::MutableHandleValue value) {
  std::string frame, operation, errorName, message, stack;
  if (!pipe_.Read(&frame)) {
    healthy_ = false;
    JS_ReportErrorASCII(cx_, "Host callback response disconnected");
    return false;
  }
  JS::RootedValue encoded(cx_);
  bool ok = false;
  {
    JSAutoRealm realm(cx_, codec_);
    JS::RootedObject response(cx_);
    JS::RootedValue responseId(cx_), success(cx_), error(cx_);
    if (!ParseRequest(cx_, frame, &response) ||
        !StringProperty(cx_, response, "op", &operation) ||
        !JS_GetProperty(cx_, response, "callId", &responseId) ||
        !JS_GetProperty(cx_, response, "ok", &success)) {
      healthy_ = false;
      return false;
    }
    if (operation != "callback-result" || !responseId.isNumber() ||
        responseId.toNumber() != static_cast<double>(callId) || !success.isBoolean()) {
      healthy_ = false;
      JS_ReportErrorASCII(cx_, "Mismatched host callback response");
      return false;
    }
    ok = success.toBoolean();
    if (ok) {
      if (!JS_GetProperty(cx_, response, "value", &encoded)) {
        healthy_ = false;
        return false;
      }
    } else {
      if (!JS_GetProperty(cx_, response, "error", &error) || !error.isObject()) {
        healthy_ = false;
        JS_ReportErrorASCII(cx_, "Invalid host callback error");
        return false;
      }
      JS::RootedObject details(cx_, &error.toObject());
      if (!StringProperty(cx_, details, "name", &errorName) ||
          !StringProperty(cx_, details, "message", &message) ||
          !StringProperty(cx_, details, "stack", &stack) ||
          errorName.size() > 128 || message.size() > 4096 || stack.size() > 16384) {
        healthy_ = false;
        if (!JS_IsExceptionPending(cx_))
          JS_ReportErrorASCII(cx_, "Host callback error exceeds protocol limits");
        return false;
      }
    }
  }
  if (!ok) return ThrowHostError(cx_, errorName, message, stack);
  if (!JS_WrapValue(cx_, &encoded) || !DecodeTyped(cx_, encoded, value)) {
    healthy_ = false;
    return false;
  }
  return true;
}
}
