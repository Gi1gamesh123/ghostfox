#pragma once
#include "resource_network/readable_tee.h"
#include "resource_network/read_completion.h"

namespace ruyijs::resource_network {
extern const ReadCompletionSteps kByteDefaultTeeSteps,kByteByob1TeeSteps,kByteByob2TeeSteps;
bool TeeByteChunk(JSContext*,JS::HandleObject,JS::HandleValue,int branch);
bool TeeByteClose(JSContext*,JS::HandleObject,JS::HandleValue,int branch);
bool TeeByteFailure(JSContext*,JS::HandleObject);
bool TeeByteAgain(JSContext*,JS::HandleObject);
}
