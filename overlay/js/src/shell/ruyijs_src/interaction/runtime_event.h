#pragma once
#include "interaction/event_creation.h"
namespace ruyijs::interaction {
enum class Kind : uint8_t;
// The original runtime factory, also used by native form/default actions.
bool FireRuntimeEvent(State&, JS::HandleObject, const char*, const char*, Iface, Kind,
    JS::HandleObject init, JS::HandleObject installed = nullptr, double time = -1,
    bool trusted = true, bool* allowed = nullptr, bool hostInput = false,
    JS::RootedObject* created = nullptr);
}
