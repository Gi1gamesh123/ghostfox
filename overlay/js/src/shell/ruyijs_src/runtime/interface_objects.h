#ifndef RUYIJS_INTERFACE_OBJECTS_H
#define RUYIJS_INTERFACE_OBJECTS_H
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs {
// Gives every interface object on |global| the WebIDL [[Prototype]]: the
// inherited interface's interface object, or %Function.prototype%; and puts its
// prototype object's "constructor" property last, as the binding defines it. The shared
// assembly runs it once per realm after that realm's last installer: the main
// window, each child or later top-level window, and each worker.
bool LinkInterfaceObjects(JSContext* cx, JS::HandleObject global);
}
#endif
