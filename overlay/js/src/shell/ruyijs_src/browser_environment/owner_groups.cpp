// Configure-only owner groups (model.h kOwnerGroups): each is merged into its
// own buffer and handed whole to the owner's port, which owns the values
// (web_canvas, web_audio, web_styling, web_rtc, web_geometry). A group set
// before the owner binds stays buffered and is pushed on binding.
#include "browser_environment/environment.h"
#include "browser_environment/schema.h"
#include "browser_environment/state.h"
#include "js/Realm.h"

namespace ruyijs::browser_environment {
bool State::MergeOwner(size_t index, const std::vector<Change>& changes) {
  JSAutoRealm realm(cx, page);
  const OwnerGroupSpec& spec = kOwnerGroups[index];
  JS::RootedObject buffer(cx, owners[index].group);
  if (!MergeGroup(cx, &buffer, spec.kind, spec.fields, spec.count, changes)) return false;
  owners[index].group = buffer;
  return true;
}
bool State::PushOwner(size_t index) {
  OwnerSlot& slot = owners[index];
  if (!slot.group.get() || !slot.port) return true;
  JSAutoRealm realm(cx, page);
  JS::RootedObject group(cx, slot.group);
  return slot.port(slot.context, group);
}
void Environment::SetOwnerPort(std::string_view group, bool (*port)(void*, JS::HandleObject),
                               void* context) {
  const OwnerGroupSpec* spec = FindOwnerGroup(std::string(group));
  if (!spec) return;
  const size_t index = size_t(spec - kOwnerGroups);
  state_->owners[index].port = port;
  state_->owners[index].context = context;
  state_->PushOwner(index);
}
}  // namespace ruyijs::browser_environment
