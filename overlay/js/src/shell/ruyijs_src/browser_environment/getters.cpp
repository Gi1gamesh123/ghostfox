#include "browser_environment/state.h"
#include "domain/browser_environment/url_value.h"
#include "jsfriendapi.h"
#include "js/Object.h"
#include "js/Wrapper.h"
#include "js/String.h"
#include <iterator>

namespace ruyijs::browser_environment {
bool State::Read(Kind kind, uint32_t field, ValueType type,
                  JS::MutableHandleValue result) {
  using UrlValue = ruyijs::domain::browser_environment::UrlValue;
  if (type == ValueType::Strings) {
    result.setObject(*languages);
    return JS_WrapValue(cx, result);
  }
  FieldValue value;
  static const char* const kBlank[] = {"about:blank", "about:", "", "", "", "blank", "", ""};
  static const char* const kSrcdoc = "about:srcdoc";
  if ((shared || worker) && kind == Kind::Location && field < 8) {
    // A child's own Location reports the document URL the runtime frame owner
    // committed through the one page-load coordinator; until then it is
    // about:blank, whose components are the constants below.
    if (frameUrl.empty()) value.text = kBlank[field];
    else {
      UrlValue url(frameUrl);
      switch (field) {
        case 0: value.text = url.href(); break;
        case 1: value.text = url.protocol(); break;
        case 2: value.text = url.host(); break;
        case 3: value.text = url.hostname(); break;
        case 4: value.text = url.port(); break;
        // Gecko's WorkerPrivate reads nsIURL::GetFilePath; Blob URIs have no
        // nsIURL path. This projection does not change URL(blob).pathname.
        case 5: value.text = worker && url.protocol() == "blob:" ? "" : url.pathname(); break;
        case 6: value.text = url.search(); break;
        default: value.text = url.hash(); break;
      }
    }
  } else if ((shared || worker) && kind == Kind::Location && field == 8 && !frameUrl.empty() &&
             frameUrl != kSrcdoc) {
    // A loaded document's origin is its own; about:blank and about:srcdoc
    // inherit the creator's, so those fall through to the shared model.
    value.text = UrlValue(frameUrl).origin();
  } else if (!(shared ? shared->model : model).Read(kind, field, &value)) {
    JS_ReportOutOfMemory(cx);  // An about:blank origin is inherited from its creator.
    return false;
  }
  if (type == ValueType::Integer) result.setInt32(value.integer);
  else if (type == ValueType::Boolean) result.setBoolean(value.boolean);
  else if (type == ValueType::Number) result.setDouble(value.number);
  else {
    JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx,
        JS::UTF8Chars(value.text.data(), value.text.size())));
    if (!text) return false;
    result.setString(text);
  }
  return true;
}
bool State::TouchCapable() {
  FieldValue value;
  if (!(shared ? shared->model : model).Read(Kind::Navigator, kNavigatorMaxTouchPoints, &value))
    return false;
  return value.integer > 0;
}
bool State::TouchEventsEnabled(bool* result) {
  if ((shared ? shared->model : model).TouchEventsEnabled(result)) return true;
  JS_ReportOutOfMemory(cx);
  return false;
}
bool State::LegacyTouchEventsEnabled() const {
  // Donor dom.w3c_touch_events.legacy_apis.enabled defaults to IS_ANDROID.
  // This Windows content runtime has no system/document/pref override input.
  return false;
}
const char* InterfaceName(Kind kind) {
  switch (kind) {
    case Kind::Navigator: return "Navigator";
    case Kind::Screen: return "Screen";
    case Kind::Location: return "Location";
    case Kind::History: return "History";
    default: return "";
  }
}
const char* MemberName(Kind kind, uint32_t index) {
  switch (kind) {
    case Kind::Navigator:
      return index < std::size(kNavigatorFields) ? kNavigatorFields[index].name : "";
    case Kind::Screen:
      return index < std::size(kScreenFields) ? kScreenFields[index].name : "";
    case Kind::Location:
      return index < std::size(kLocationFields) ? kLocationFields[index].name : "";
    case Kind::History: return "length";
    default: return "";
  }
}
static State* Receiver(JSContext* cx, const JS::CallArgs& args, Kind kind) {
  auto* state = static_cast<State*>(
      js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  State* receiver = state ? ReceiverState(args, ClassFor(kind, state->worker)) : nullptr;
  if (!receiver) {
    ReceiverError(cx, args, ClassFor(kind, state ? state->worker : false)->name);
    return nullptr;
  }
  return receiver;
}
bool Getter(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int32_t slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  Kind kind = Kind(slot / 32);
  uint32_t index = uint32_t(slot % 32);
  State* state = Receiver(cx, args, kind);
  if (!state) return false;
  // Reported after receiver validation, so an invalid receiver still throws
  // from the same place and records nothing.
  state->Observe(ClassFor(kind, state->worker)->name, MemberName(kind, index), int(ApiAccess::Read));
  ValueType type = kind == Kind::Navigator ? kNavigatorFields[index].type :
                   kind == Kind::Location ? ValueType::String : ValueType::Integer;
  return state->Read(kind, index, type, args.rval());
}
bool LocationString(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  State* state = Receiver(cx, args, Kind::Location);
  if (!state) return false;
  state->Observe(ClassFor(Kind::Location, state->worker)->name, "toString", int(ApiAccess::Call));
  return state->Read(Kind::Location, 0, ValueType::String, args.rval());
}
}
