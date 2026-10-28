#pragma once
#include "execution_scheduling/realm.h"
namespace ruyijs {
bool NavigationReplayRequest(JSContext*, execution_scheduling::Realm&, JS::HandleObject,
                             bool configure, bool frozen, JS::MutableHandleValue);
}
