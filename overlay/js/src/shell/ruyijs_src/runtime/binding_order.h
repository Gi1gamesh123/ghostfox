// Own-key order of an interface prototype assembled by several owners: each
// configurable own property is re-defined, unchanged, in the generated
// binding's order. Names a realm did not install are skipped; keys outside the
// table keep their relative order after it, then "constructor", then symbols.
// Both entry points enter the target's realm themselves, so a caller in the
// controller realm never allocates the page's shapes in the controller zone.
#ifndef RUYIJS_RUNTIME_BINDING_ORDER_H
#define RUYIJS_RUNTIME_BINDING_ORDER_H
#include <cstddef>
#include "js/RootingAPI.h"

namespace ruyijs {
bool OrderOwnKeys(JSContext* cx, JS::HandleObject target, const char* const* order, size_t count);
// Every interface prototype reachable from the global's own interface objects,
// in the donor bindings' member order (binding_order_table.cpp). Run once per
// realm after all owners installed.
bool OrderInterfacePrototypes(JSContext* cx, JS::HandleObject global);
}
#endif
