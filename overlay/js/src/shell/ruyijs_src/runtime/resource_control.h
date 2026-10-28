#ifndef RUYIJS_RESOURCE_CONTROL_H
#define RUYIJS_RESOURCE_CONTROL_H
#include "resource_network/realm.h"

namespace ruyijs {
class ResourceControl final {
 public:
  ResourceControl(JSContext* cx, resource_network::Model& owner) : cx_(cx), owner_(owner) {}
  bool Request(JS::HandleObject request, JS::MutableHandleValue result);
 private:
  JSContext* cx_;
  resource_network::Model& owner_;
};
}
#endif
