#include "browser_environment/state.h"
#include "browser_environment/worker_profile.h"
#include "js/JSON.h"
#include "js/String.h"
#include <iterator>

namespace ruyijs::browser_environment {
bool State::ApplyWorkerProfile(std::string_view profile) {
  // Empty startup metadata is used only by the private native process fixture.
  // Public Worker construction always exports the creator's complete profile.
  if (profile.empty()) return Baseline();
  if (profile.size() > kWorkerProfileLimit)
    return Error(cx, "Worker environment exceeds its byte limit", JSEXN_RANGEERR);
  JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx,
      JS::UTF8Chars(profile.data(), profile.size())));
  JS::RootedValue value(cx);
  if (!text || !JS_ParseJSON(cx, text, &value)) return false;
  if (!value.isObject()) return Error(cx, "Worker environment must be an object");
  JS::RootedObject object(cx, &value.toObject());
  if (!ImportWorkerGroups(object) || !ApplyWorkerContext(object)) return false;
  std::vector<Change> changes;
  if (!Profile(cx, object, false, &changes)) return false;
  size_t expected = 1;  // The original locale.timezone field.
  for (const auto& field : kNavigatorFields) expected += field.worker;
  if (changes.size() != expected) return Error(cx, "Worker environment is incomplete");
  for (const auto& change : changes) {
    if (change.kind == Kind::Locale && change.field == 0) continue;
    if (change.kind != Kind::Navigator || change.field >= std::size(kNavigatorFields) ||
        !kNavigatorFields[change.field].worker)
      return Error(cx, "Worker environment has an unexpected field");
  }
  return ApplyChanges(changes);
}
}
