#include "runtime/user_activation_ports.h"
#include "runtime/close_watcher_ports.h"
#include "runtime/browsing_context.h"
#include "runtime/top_level.h"
#include "execution_scheduling/realm.h"

namespace ruyijs {
BrowsingContext* Frames::FindCurrentDocument(const document_model::Realm& document) const {
  if (!topLevel_) return nullptr;
  auto* top = &topLevel_->Active();
  if (top->document.get() == &document) return top;
  for (const auto& child : children_) {
    if (child->document.get() != &document) continue;
    auto* ancestor = child.get();
    while (ancestor->parent) {
      if (!Live(*ancestor)) return nullptr;
      ancestor = ancestor->parent;
    }
    return ancestor == top ? child.get() : nullptr;
  }
  return nullptr;
}
bool Frames::NotifyActivation(BrowsingContext& source, double now) {
  // Original live frame topology; no copied child list or page-visible reads.
  if (FindCurrentDocument(*source.document) != &source) return true;
  // Existing frame quota bounds this stack-only recipient list. Finish all
  // fallible origin reads before mutating any activation value.
  BrowsingContext* recipients[kLimit + 1];
  size_t count = 0;
  for (auto* at = &source; at; at = at->parent) recipients[count++] = at;
  for (const auto& child : children_) {
    if (child.get() == &source || !DescendsFrom(child.get(), &source)) continue;
    if (FindCurrentDocument(*child->document) != child.get()) continue;
    bool same = false;
    if (!source.environment->SameOrigin(*child->environment, &same)) return false;
    if (same) recipients[count++] = child.get();
  }
  for (size_t i = 0; i < count; ++i) {
    recipients[i]->environment->NotifyActivation(now);
    recipients[i]->interaction->NotifyCloseWatcherActivation();
  }
  return true;
}
void Frames::ConsumeHistoryActivation(BrowsingContext& source) {
  if (FindCurrentDocument(*source.document) != &source) return;
  auto* top = &topLevel_->Active();
  top->environment->ConsumeHistoryActivation();
  for (const auto& child : children_)
    if (DescendsFrom(child.get(), top) && FindCurrentDocument(*child->document) == child.get())
      child->environment->ConsumeHistoryActivation();
}
void Frames::UnbindActivation() {
  for (const auto& child : children_) UnbindUserActivation(*child);
  for (const auto& child : retired_) UnbindUserActivation(*child);
}
void BindUserActivation(BrowsingContext& target, execution_scheduling::Realm& scheduling) {
  BindCloseWatcher(target);
  target.scheduling = &scheduling;
  target.environment->SetActivationClock([](void* context) {
    return static_cast<execution_scheduling::Realm*>(context)->ClockSeconds() * 1000.0;
  }, &scheduling);
  target.interaction->SetInputActivationPort([](void* context, JS::HandleObject inputTarget) {
    auto& owner = *static_cast<BrowsingContext*>(context);
    // Original wrapper identity follows adoption; generic EventTarget and nodes
    // of a superseded Document carry no current input document.
    auto* document = owner.document->InputDocument(inputTarget);
    if (!document || !owner.scheduling || !owner.frames) return true;
    auto* source = owner.frames->FindCurrentDocument(*document);
    return !source || owner.frames->NotifyActivation(*source,
        owner.scheduling->ClockSeconds() * 1000.0);
  }, &target);
}
void UnbindUserActivation(BrowsingContext& target) {
  UnbindCloseWatcher(target);
  target.interaction->SetInputActivationPort(nullptr, nullptr);
  target.environment->SetActivationClock(nullptr, nullptr);
}
}
