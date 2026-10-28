#pragma once
#include "jsapi.h"

namespace ruyijs::resource_network {
// Internal ResourceNetwork completion steps. Tables have static lifetime and
// contain only native function pointers; every JS edge is in the traced record.
using ReadCompletionStep=bool (*)(JSContext*,JS::HandleObject owner,
                                 JS::HandleObject operation,JS::HandleValue);
struct ReadCompletionSteps {
  ReadCompletionStep chunk;
  ReadCompletionStep close;
  ReadCompletionStep error;
};
bool NewReadCompletion(JSContext*,JS::HandleObject owner,JS::HandleObject operation,
                       const ReadCompletionSteps&,JS::MutableHandleObject);
bool IsNativeReadCompletion(JSObject*);
bool FailNativeReadCompletion(JSContext*,JS::HandleObject target);
bool CompleteRead(JSContext*,JS::HandleObject target,bool done,JS::HandleValue,bool byob=false);
bool RejectReadCompletion(JSContext*,JS::HandleObject target,JS::HandleValue);
}
