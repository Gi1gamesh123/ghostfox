#include "document_model/model_internal.h"
#include "domain/document_model/anchor_values.h"

namespace ruyijs::document_model {
Status Model::AnchorPolicy(NodeKey key, DomString* out) const noexcept {
  return Protect([&] { *out = domain::anchor_policy(State().element(key).get_attribute16(u"referrerpolicy").value_or(u"")); });
}
Status Model::SetAnchorPolicy(NodeKey key, DomStringView value) noexcept {
  DomString text;
  auto status = Protect([&] { text = domain::anchor_policy_value(value); });
  return status.code == ErrorCode::None ? SetAttribute(key, u"referrerpolicy", text) : status;
}
}
