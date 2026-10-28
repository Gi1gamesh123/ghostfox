// The FileReader installer. The prototype is created with EventTarget.prototype as
// its [[Prototype]], so the reader is a real EventTarget; every method, accessor
// and handler attribute goes through the donor trace registry.
#include "web_platform/state.h"

#include "interaction/realm.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::web_platform {
namespace {
struct MethodDef {
  const char* name;
  unsigned length;
  int32_t slot;
};

constexpr MethodDef kMethods[] = {
    {"readAsText", 1, kReaderText},
    {"readAsArrayBuffer", 1, kReaderArrayBuffer},
    {"readAsDataURL", 1, kReaderDataURL},
    {"readAsBinaryString", 1, kReaderBinaryString},
    {"abort", 0, kReaderAbort}};

struct FieldDef {
  const char* name;
  int32_t slot;
};

constexpr FieldDef kFields[] = {
    {"readyState", kReaderReadyStateField},
    {"result", kReaderResultField},
    {"error", kReaderErrorField}};

// The six handler attributes the interface exposes, in slot order.
constexpr const char* kHandlers[] = {"onloadstart", "onprogress", "onload",
                                     "onabort", "onerror", "onloadend"};

struct ConstantDef {
  const char* name;
  int32_t value;
};

constexpr ConstantDef kConstants[] = {{"EMPTY", kReaderReadyEmpty},
                                      {"LOADING", kReaderReadyLoading},
                                      {"DONE", kReaderReadyDone}};
}  // namespace

bool InstallFileReader(State& state, PageHelpers& helpers) {
  JSContext* cx = state.cx;
  WebKit& kit = state.kit;
  if (!state.events) return WebKit::Fail(cx, "FileReader requires the interaction owner");
  JS::RootedObject event_prototype(cx, state.events->prototype());
  if (!event_prototype ||
      !kit.Define("FileReader", &kReaderClass, event_prototype, FileReaderConstructor,
                  &state.reader))
    return false;
  for (const MethodDef& method : kMethods)
    if (!kit.Method(state.reader, "FileReader", method.name, FileReaderMethod,
                    method.length, method.slot))
      return false;
  for (const FieldDef& field : kFields)
    if (!kit.Accessor(state.reader, "FileReader", field.name, FileReaderGetter, nullptr,
                      field.slot))
      return false;
  for (int32_t index = 0; index < 6; ++index)
    if (!kit.Accessor(state.reader, "FileReader", kHandlers[index], ReaderHandlerGet,
                      ReaderHandlerSet, index))
      return false;
  const unsigned flags = JSPROP_READONLY | JSPROP_PERMANENT | JSPROP_ENUMERATE;
  for (const ConstantDef& constant : kConstants)
    if (!JS_DefineProperty(cx, state.reader.constructor, constant.name, constant.value, flags) ||
        !JS_DefineProperty(cx, state.reader.prototype, constant.name, constant.value,
                           flags & ~JSPROP_PERMANENT))  // made permanent by the binding order pass
      return false;
  return helpers.Constructor("FileReader", state.reader.constructor) &&
         helpers.Interface("FileReader", state.reader.prototype);
}
}  // namespace ruyijs::web_platform
