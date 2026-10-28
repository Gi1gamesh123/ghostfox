#include "runtime/user_activation_ports.h"
#include "runtime/top_level.h"
#include "runtime/runtime.h"
#include "jsapi.h"

namespace ruyijs {
void TopLevel::UnbindResourceCallbacks() {
  for (const auto& context:later_)
    if (context->resources) {
      context->resources->SetCallbackEntryPort(nullptr,nullptr);
      context->resources->SetKeepaliveTaskPort(nullptr,nullptr);
    }
}
TopLevel::~TopLevel() {
  while (!later_.empty()) later_.pop_back();
}
bool TopLevel::Advance(BrowsingContext** created) {
  *created = nullptr;
  if (later_.size() + 1 >= kLimit) {
    JS_ReportErrorASCII(cx_, "top-level document limit exceeded");
    return false;
  }
  BrowsingContext& previous = *active_;
  JS::RootedObject global(cx_, CreatePage(cx_, previous.page));
  if (!global) return false;
  // Kept from here on even if assembly fails half way: objects of a partly
  // installed realm may already point at its owners.
  later_.push_back(std::make_unique<BrowsingContext>(cx_, global));
  BrowsingContext& next = *later_.back();
  // The same ordered installers as the first document, then the child-realm
  // Scheduling and resource surfaces over this Context's one clock, queues,
  // catalog and NetLog, before any script of the new document can run.
  if (!next.BeginSuccessor(previous) || !next.Finish(*previous.frames)) return false;
  next.observers->CopyPortsFrom(*previous.observers);
  if (!scheduling_.Run(next) || !resource_.Run(next)) return false;
  next.document->CopyScriptPortFrom(*previous.document);
  next.document->CopyFormPortsFrom(*previous.document);
  if (!document_.Run(next)) return false;
  if (retire_) retire_(retireContext_, previous);
  active_ = &next;
  page_ = global;
  if (!host_.Run(next)) return false;
  *created = &next;
  return true;
}
void TopLevel::UnbindActivation() {
  for (const auto& context : later_) UnbindUserActivation(*context);
}
void TopLevel::Close() {
  for (const auto& context : later_)
    if (retire_ && context.get() == active_) retire_(retireContext_, *active_);
  scheduling_ = resource_ = document_ = host_ = Port{};
  retire_ = nullptr;
  retireContext_ = nullptr;
}
}
