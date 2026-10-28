#pragma once
#include "resource_network/realm.h"

namespace ruyijs {
// Host RPC only: no page function, property or lexical binding is installed.
bool CookieRequest(JSContext*, resource_network::Model&, JS::HandleObject,
                   JS::MutableHandleValue);
}
