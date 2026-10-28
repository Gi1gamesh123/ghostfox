#include "execution_scheduling/state.h"
namespace ruyijs::execution_scheduling {
bool Realm::ConfigureNavigationReplay(domain::execution_scheduling::NavigationReplayDocuments&& documents) {
  if (state_->frozen) return Error(state_->cx, "Navigation replay configuration is closed after execution");
  state_->navigationReplay.swap(documents);
  return true;
}
const domain::execution_scheduling::NavigationReplayDocuments& Realm::NavigationReplayStatus() const {
  return state_->navigationReplay;
}
bool State::ApplyNavigationReplay(NavigationTiming& timing, size_t* selected) {
  *selected = navigationReplay.size();
  bool known = false;
  for (size_t index = 0; index < navigationReplay.size(); ++index) {
    const auto& entry = navigationReplay[index];
    if (entry.url != timing.name) continue;
    known = true;
    if (entry.bound) continue;
    const auto& fields = entry.fields;
    timing.fetch_start = timing.request_start;
    if (!timing.transfer_size) timing.transfer_size = timing.encoded_size ? timing.encoded_size + 300 : 0;
    if (fields.request_start) timing.request_start = *fields.request_start;
    if (fields.response_start) timing.response_start = *fields.response_start;
    if (fields.response_end) timing.response_end = *fields.response_end;
    if (fields.transfer_size) timing.transfer_size = *fields.transfer_size;
    if (fields.encoded_size) timing.encoded_size = *fields.encoded_size;
    if (fields.decoded_size) timing.decoded_size = *fields.decoded_size;
    timing.recorded_load_event_end = fields.load_event_end;
    *selected = index;
    return true;
  }
  return !known || Error(cx, "Recorded navigation document occurrences exhausted");
}
}
