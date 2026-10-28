#include "document_model/state.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
bool Realm::ThrowException(const char* name, const char* message) {
  JSAutoRealm realm(state_->cx, state_->page);
  return ThrowDOM(*state_, name, message);
}
}
