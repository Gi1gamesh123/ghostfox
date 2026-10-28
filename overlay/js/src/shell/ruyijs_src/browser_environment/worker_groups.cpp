// The creator's buffered owner configuration groups travel with a Worker's
// startup profile, so the Worker realm's WebGL, WebGPU, Permissions, Canvas,
// Fonts, Audio, RTC and media-feature owners read the same configuration as
// the creating Window (docs/owners/WORKER.md, 2026-10-21). Each group is
// buffered in the Worker State and flushed when its owner binds its port.
#include "browser_environment/state.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::browser_environment {
namespace {
struct Buffered { const char* name; JS::PersistentRootedObject State::*group; };
const Buffered kBuffered[] = {{"webgl", &State::webglGroup}, {"webgpu", &State::gpuGroup},
                              {"permissions", &State::permissionsGroup}};
}  // namespace
bool State::ExportWorkerGroups(JS::HandleObject output) {
  JS::RootedObject groups(cx, JS_NewPlainObject(cx));
  if (!groups) return false;
  for (const auto& entry : kBuffered) {
    JS::RootedObject group(cx, (this->*entry.group).get());
    if (group && !JS_DefineProperty(cx, groups, entry.name, group, JSPROP_ENUMERATE)) return false;
  }
  for (size_t index = 0; index < kOwnerGroupCount; ++index) {
    JS::RootedObject group(cx, owners[index].group.get());
    if (group && !JS_DefineProperty(cx, groups, kOwnerGroups[index].name, group, JSPROP_ENUMERATE)) return false;
  }
  return JS_DefineProperty(cx, output, "ownerGroups", groups, JSPROP_ENUMERATE);
}
// Takes `ownerGroups` off the parsed profile (the field validation sees only
// the navigator/locale/context groups) and buffers each known group.
bool State::ImportWorkerGroups(JS::HandleObject profile) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, profile, "ownerGroups", &value) || !JS_DeleteProperty(cx, profile, "ownerGroups"))
    return false;
  if (value.isUndefined()) return true;
  if (!value.isObject()) return Error(cx, "Worker owner groups must be an object");
  JS::RootedObject groups(cx, &value.toObject());
  JS::RootedValue group(cx);
  for (const auto& entry : kBuffered) {
    if (!JS_GetProperty(cx, groups, entry.name, &group)) return false;
    if (group.isObject()) this->*entry.group = &group.toObject();
  }
  for (size_t index = 0; index < kOwnerGroupCount; ++index) {
    if (!JS_GetProperty(cx, groups, kOwnerGroups[index].name, &group)) return false;
    if (group.isObject()) owners[index].group = &group.toObject();
  }
  return true;
}
}  // namespace ruyijs::browser_environment
