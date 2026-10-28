#include "document_model/state.h"

namespace ruyijs::document_model {
Wrapper* ElementReceiver(JSContext* cx, JS::HandleValue value) {
  return NodeReceiver(cx, value, NodeKind::Element);
}
}
