#include "browser_environment/model_internal.h"
#include <memory>

namespace ruyijs::browser_environment {
// History mutations follow Model::Configure's copy-on-write atomicity: a failed
// allocation leaves the committed aggregate untouched.
bool Model::HistoryPush(const std::string& url, const std::vector<uint8_t>& state) noexcept {
  if (!impl_) return false;
  try {
    auto candidate = std::make_unique<Impl>(*impl_);
    candidate->aggregate.push_history(url, state);
    delete impl_;
    impl_ = candidate.release();
    return true;
  } catch (...) { return false; }
}
bool Model::HistoryReplace(const std::string& url, const std::vector<uint8_t>& state) noexcept {
  if (!impl_) return false;
  try {
    auto candidate = std::make_unique<Impl>(*impl_);
    candidate->aggregate.replace_history(url, state);
    delete impl_;
    impl_ = candidate.release();
    return true;
  } catch (...) { return false; }
}
bool Model::HistoryGo(int32_t delta, bool* changed) noexcept {
  if (!impl_ || !changed) return false;
  try {
    auto candidate = std::make_unique<Impl>(*impl_);
    if (!candidate->aggregate.go_history(delta)) { *changed = false; return true; }
    delete impl_;
    impl_ = candidate.release();
    *changed = true;
    return true;
  } catch (...) { return false; }
}
bool Model::HistoryState(std::vector<uint8_t>* state) const noexcept {
  if (!impl_ || !state) return false;
  try { *state = impl_->aggregate.history().current().state; return true; }
  catch (...) { return false; }
}
bool Model::HistoryIndex(int32_t* index) const noexcept {
  if (!impl_ || !index) return false;
  try { *index = static_cast<int32_t>(impl_->aggregate.history().index()); return true; }
  catch (...) { return false; }
}
bool Model::HistoryEntries(std::vector<HistoryView>* entries, int32_t* index) const noexcept {
  if (!impl_ || !entries || !index) return false;
  try {
    const auto& history = impl_->aggregate.history();
    entries->clear();
    for (const auto& entry : history.entries())
      entries->push_back({entry.id, entry.revision, entry.url, entry.same_document});
    *index = static_cast<int32_t>(history.index());
    return true;
  } catch (...) { return false; }
}
bool Model::HistoryNavigationState(int32_t index, std::vector<uint8_t>* state) const noexcept {
  if (!impl_ || !state) return false;
  try {
    const auto& entries = impl_->aggregate.history().entries();
    if (index < 0 || size_t(index) >= entries.size()) { state->clear(); return true; }
    *state = entries[size_t(index)].navigation_state;
    return true;
  } catch (...) { return false; }
}
bool Model::SetHistoryNavigationState(const std::vector<uint8_t>& state) noexcept {
  if (!impl_) return false;
  try {
    auto candidate = std::make_unique<Impl>(*impl_);
    candidate->aggregate.set_navigation_state(state);
    delete impl_;
    impl_ = candidate.release();
    return true;
  } catch (...) { return false; }
}
bool Model::HistoryCommit(bool replace, const std::string& url, const std::vector<uint8_t>& state,
                          const std::vector<uint8_t>& navigation) noexcept {
  if (!impl_) return false;
  try {
    auto candidate = std::make_unique<Impl>(*impl_);
    if (replace) candidate->aggregate.replace_history(url, state, navigation);
    else candidate->aggregate.push_history(url, state, navigation);
    delete impl_;
    impl_ = candidate.release();
    return true;
  } catch (...) { return false; }
}
bool Model::HistoryCurrentUrl(std::string* url) const noexcept {
  if (!impl_ || !url) return false;
  try { *url = impl_->aggregate.history().current().url; return true; }
  catch (...) { return false; }
}
// A navigation commit reuses the two aggregate operations the Location
// interface needs: navigate_to appends a document entry, replace_navigation
// replaces the current one and keeps the length. Both are copy-on-write, so a
// failed allocation leaves the committed aggregate untouched.
bool Model::Navigate(const std::string& url, bool replace) noexcept {
  if (!impl_) return false;
  try {
    auto candidate = std::make_unique<Impl>(*impl_);
    if (replace) candidate->aggregate.replace_navigation(url);
    else candidate->aggregate.navigate_to(url);
    delete impl_;
    impl_ = candidate.release();
    return true;
  } catch (...) { return false; }
}
bool Model::ScrollRestoration(bool* manual) const noexcept {
  if (!impl_ || !manual) return false;
  try {
    *manual = impl_->aggregate.history().scroll_restoration() ==
              ruyijs::domain::browser_environment::ScrollRestoration::Manual;
    return true;
  } catch (...) { return false; }
}
bool Model::SetScrollRestoration(bool manual) noexcept {
  if (!impl_) return false;
  try {
    auto candidate = std::make_unique<Impl>(*impl_);
    candidate->aggregate.set_history_scroll_restoration(manual);
    delete impl_;
    impl_ = candidate.release();
    return true;
  } catch (...) { return false; }
}
}  // namespace ruyijs::browser_environment