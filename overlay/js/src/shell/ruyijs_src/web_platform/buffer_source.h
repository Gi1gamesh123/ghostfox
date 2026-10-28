#pragma once
#include "js/RootingAPI.h"
namespace ruyijs::web_platform {
// Strict WebIDL stream union, or the preserved original TextDecoder policy.
bool PlatformBufferSource(JSContext*,JS::HandleValue,JS::MutableHandleObject,bool streamCaller);
}
