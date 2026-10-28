#include "runtime/host_bridge.h"
#include "runtime/pipe.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/ValueArray.h"

namespace ruyijs {
bool HostBridge::EncodeEvent(const JS::CallArgs& args, JS::HandleString name,
                              uint64_t callId, std::string* frame) {
  JS::RootedObject values(cx_, JS::NewArrayObject(cx_, args.length()));
  if (!values) return false;
  for (unsigned index = 0; index < args.length(); ++index)
    if (!JS_DefineElement(cx_, values, index, args[index], JSPROP_ENUMERATE))
      return false;
  JSAutoRealm realm(cx_, codec_);
  JS::RootedValueArray<3> arguments(cx_);
  arguments[0].setDouble(static_cast<double>(callId));
  arguments[1].setString(name);
  arguments[2].setObject(*values);
  if (!JS_WrapValue(cx_, arguments[1]) || !JS_WrapValue(cx_, arguments[2])) return false;
  JS::RootedValue result(cx_);
  if (!JS_CallFunctionName(cx_, codec_, "callback", arguments, &result)) return false;
  if (!result.isString()) {
    JS_ReportErrorASCII(cx_, "Invalid host callback encoding");
    return false;
  }
  JS::RootedString text(cx_, result.toString());
  if (!ToUTF8(cx_, text, frame)) return false;
  if (frame->size() > Pipe::kMaxFrame) {
    JS_ReportErrorASCII(cx_, "Host callback exceeds 16 MiB protocol limit");
    return false;
  }
  return true;
}

bool HostBridge::Call(const JS::CallArgs& args) {
  if (inFlight_) {
    JS_ReportErrorASCII(cx_, "Nested host callback is not supported");
    return false;
  }
  if (nextCallId_ >= 9007199254740991ULL || args.length() > 100000) {
    JS_ReportErrorASCII(cx_, "Host callback limit exceeded");
    return false;
  }
  struct CallScope {
    bool& active;
    explicit CallScope(bool& value) : active(value) { active = true; }
    ~CallScope() { active = false; }
  } scope(inFlight_);
  uint64_t callId = ++nextCallId_;
  JS::RootedString name(cx_,
      js::GetFunctionNativeReserved(&args.callee(), 1).toString());
  std::string frame;
  if (!EncodeEvent(args, name, callId, &frame)) return false;
  if (!pipe_.Write(frame)) {
    healthy_ = false;
    JS_ReportErrorASCII(cx_, "Host callback transport disconnected");
    return false;
  }
  return ReadReply(callId, args.rval());
}
}
