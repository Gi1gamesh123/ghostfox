/* ruyi-clone stub: generated binding-order table.
   Empty table = keep installer order (our native installers already define
   members in the original's order). Populate from probe blueprints later. */
#ifndef RUYIJS_FIREFOX_BINDING_ORDER_H
#define RUYIJS_FIREFOX_BINDING_ORDER_H
#include <cstddef>

namespace ruyijs {
struct BindingOrderEntry {
  const char* name;
  const char* const* members;
  size_t count;
  size_t constants; /* leading read-only constant block */
};
}  // namespace ruyijs

inline const ruyijs::BindingOrderEntry kBindingOrders[] = {};

#endif
