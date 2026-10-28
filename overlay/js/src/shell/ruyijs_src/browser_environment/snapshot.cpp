#include "browser_environment/state.h"
#include "browser_environment/worker_profile.h"
#include "runtime/runtime.h"
#include "firefox_environment_profile.h"
#include "js/JSON.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include <cstring>
#include <iterator>

namespace ruyijs::browser_environment {
bool ParseBaseline(JSContext* cx, JS::MutableHandleValue result) {
  JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx,
      JS::UTF8Chars(kFirefoxEnvironmentProfile, std::strlen(kFirefoxEnvironmentProfile))));
  return text && JS_ParseJSON(cx, text, result);
}
static bool Group(State& state, JS::HandleObject output, const char* name,
                   Kind kind, const Field* fields, size_t count, bool workerOnly = false) {
  JSContext* cx = state.cx;
  JS::RootedObject group(cx, JS_NewPlainObject(cx));
  if (!group) return false;
  for (size_t index = 0; index < count; ++index) {
    if (workerOnly && !fields[index].worker) continue;
    JS::RootedValue value(cx);
    if (!state.Read(kind, uint32_t(index), fields[index].type, &value) ||
        !JS_DefineProperty(cx, group, fields[index].name, value, JSPROP_ENUMERATE)) return false;
  }
  return JS_DefineProperty(cx, output, name, group, JSPROP_ENUMERATE);
}
namespace {
struct WorkerJSON { std::u16string text; bool overflow = false; };
bool AppendWorkerJSON(const char16_t* value, uint32_t size, void* context) {
  auto& output = *static_cast<WorkerJSON*>(context);
  if (size > kWorkerProfileLimit - output.text.size()) {
    output.overflow = true;
    return false;
  }
  output.text.append(value, size);
  return true;
}
}
bool Environment::ExportWorkerProfile(std::string* result) {
  State& state = *state_;
  if (!state.Baseline()) return false;
  JS::RootedObject output(state.cx, JS_NewPlainObject(state.cx));
  if (!output || !Group(state, output, "navigator", Kind::Navigator, kNavigatorFields,
                        std::size(kNavigatorFields), true) ||
      !Group(state, output, "locale", Kind::Locale, kLocaleFields, 1) ||
      !state.ExportWorkerContext(output) || !state.ExportWorkerGroups(output)) return false;
  WorkerJSON json;
  if (!JS::ToJSONMaybeSafely(state.cx, output, AppendWorkerJSON, &json))
    return json.overflow ? Error(state.cx, "Worker environment exceeds its byte limit", JSEXN_RANGEERR) : false;
  JS::RootedString text(state.cx, JS_NewUCStringCopyN(state.cx, json.text.data(), json.text.size()));
  if (!text || !ToUTF8(state.cx, text, result)) return false;
  return result->size() <= kWorkerProfileLimit ||
      Error(state.cx, "Worker environment exceeds its byte limit", JSEXN_RANGEERR);
}
bool Environment::Describe(JS::MutableHandleValue result) {
  State& state = *state_;
  if (!state.Baseline()) return false;
  JS::RootedObject output(state.cx, JS_NewPlainObject(state.cx));
  const Field history{"length", ValueType::Integer};
  if (!output ||
      !Group(state, output, "navigator", Kind::Navigator, kNavigatorFields,
               std::size(kNavigatorFields)) ||
      !Group(state, output, "screen", Kind::Screen, kScreenFields,
               std::size(kScreenFields)) ||
      !Group(state, output, "viewport", Kind::Viewport, kViewportFields,
               std::size(kViewportFields)) ||
      !Group(state, output, "location", Kind::Location, kLocationFields,
               std::size(kLocationFields)) ||
      !Group(state, output, "locale", Kind::Locale, kLocaleFields, 1) ||
      !Group(state, output, "storage", Kind::Storage, kStorageFields,
             std::size(kStorageFields)) ||
      !Group(state, output, "history", Kind::History, &history, 1)) return false;
  // The webgl group's effective values come from the WebGL owner, never from a
  // copy here. It is present whenever the owner is installed.
  if (state.graphicsDescribe) {
    JS::RootedValue webgl(state.cx);
    if (!state.graphicsDescribe(state.graphicsDescribeContext, &webgl) ||
        !JS_DefineProperty(state.cx, output, "webgl", webgl, JSPROP_ENUMERATE))
      return false;
  }
  // The webgpu group's effective values come from the WebGPU owner, never from
  // a copy here. It is present whenever the owner is installed.
  if (state.gpuDescribe) {
    JS::RootedValue webgpu(state.cx);
    if (!state.gpuDescribe(state.gpuDescribeContext, &webgpu) ||
        !JS_DefineProperty(state.cx, output, "webgpu", webgpu, JSPROP_ENUMERATE))
      return false;
  }
  // The speech group has no owner describe port: the environment model holds
  // the effective list and builds the group object in kSpeechFields shape, so
  // the value reported here is exactly what PushSpeech hands to the owner.
  JS::RootedObject speech(state.cx);
  if (!state.EffectiveSpeech(&speech) ||
      !JS_DefineProperty(state.cx, output, "speech", speech, JSPROP_ENUMERATE))
    return false;
  if (state.mediaDescribe) {
    JS::RootedValue media(state.cx);
    if (!state.mediaDescribe(state.mediaDescribeContext, &media) ||
        !JS_DefineProperty(state.cx, output, "media", media, JSPROP_ENUMERATE))
      return false;
  }
  for (size_t owner = 0; owner < kOwnerGroupCount; ++owner) {
    if (!state.owners[owner].group.get()) continue;
    JS::RootedObject group(state.cx, state.owners[owner].group);
    if (!JS_DefineProperty(state.cx, output, kOwnerGroups[owner].name, group, JSPROP_ENUMERATE))
      return false;
  }
  if (state.permissionsDescribe) {
    JS::RootedValue permissions(state.cx);
    if (!state.permissionsDescribe(state.permissionsDescribeContext, &permissions) ||
        !JS_DefineProperty(state.cx, output, "permissions", permissions, JSPROP_ENUMERATE))
      return false;
  }
  result.setObject(*output);
  return true;
}
}
