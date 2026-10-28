// HTMLStyleElement projects one existing CSSOM sheet; it owns no CSS state.
#include "document_model/html_control.h"
namespace ruyijs::document_model {
bool StyleElementAccess(Wrapper& entry, StyleElementAction action, bool disabled,
                         JS::MutableHandleValue result) {
  auto& state = *entry.owner;
  if (state.styleElement)
    return state.styleElement(state.styleContext, entry.key.document, entry.key.node,
                               action, disabled, result);
  if (action == StyleElementAction::Sheet) result.setNull();
  else if (action == StyleElementAction::ReadDisabled) result.setBoolean(false);
  else result.setUndefined();
  return true;
}
}
