#pragma once
#include "js/RootingAPI.h"

namespace ruyijs::resource_network {
struct State;
struct RequestDraft;
enum class NativeFetchOutcome { Complete,NetworkFailure,Aborted,Retired,NativeFailure };
struct NativeFetchSteps {
  bool (*finish)(JSContext*,JS::HandleObject target,NativeFetchOutcome,
                 JS::HandleObject response,JS::HandleValue reason);
};
// Validated Request snapshot; immutable steps have static storage duration.
bool FetchNative(State&,const RequestDraft&,JS::HandleValue signal,
                 JS::HandleObject target,const NativeFetchSteps&);
void RetireNativeFetches(State&);
void DetachNativeFetches(State&);
}
