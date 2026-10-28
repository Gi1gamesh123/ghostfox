#pragma once
#include "js/RootingAPI.h"

namespace ruyijs::resource_network {
struct State;
// Per-invocation result, never stored on a stream or an asynchronous record.
enum class CallbackResult { Success, EntryRejected, Failure };
inline CallbackResult CallbackOutcome(bool ok) {
  return ok ? CallbackResult::Success : CallbackResult::Failure;
}
inline constexpr const char* kDisabledCallbackMessage =
    "Refusing to execute function from global in which script is disabled.";
// Borrowed only on the synchronous algorithm stack. Direct bindings create
// ErrorResult exceptions in their method realm; native reactions use the owner.
struct CallbackContext {
  const char* message = kDisabledCallbackMessage;
  State* exceptionOwner = nullptr;
};
CallbackResult CheckCallbackEntry(State&, JS::HandleValue callback,
                                 CallbackContext = {});
}
