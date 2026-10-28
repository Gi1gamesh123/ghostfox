#include "document_model/model_internal.h"

namespace ruyijs::document_model {
Status Model::DialogReturnValue(NodeKey key, DomString* result) const noexcept {
  return Protect([&] { *result = State().element(key).dialog_return_value(); });
}
Status Model::SetDialogReturnValue(NodeKey key, DomStringView value) noexcept {
  return Protect([&] {
    auto& element = State().element(key);
    State().budget(0, value.size(), element.dialog_return_value().size());
    element.set_dialog_return_value(DomString(value));
  });
}
Status Model::DialogAction(NodeKey key, bool close, const std::optional<DomString>& value) noexcept {
  bool open = false;
  auto status = Protect([&] {
    open = State().element(key).get_attribute16(u"open").has_value();
    if (open && !close)
      throw domain::DomError(domain::DomErrorCode::InvalidState, "Dialog is already open");
  });
  if (status.code != ErrorCode::None || (close && !open)) return status;
  if (!close) return SetAttribute(key, u"open", u"");
  if (value) {
    status = SetDialogReturnValue(key, *value);
    if (status.code != ErrorCode::None) return status;
  }
  return RemoveAttribute(key, u"open");
}
}
