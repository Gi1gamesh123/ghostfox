#include "document_model/model_internal.h"
#include "domain/browser_environment/url_value.h"

namespace ruyijs::document_model {
Status Model::ReadDocumentMetadata(NodeKey key, DocumentMetadataField field,
                                  DomStringView url, DomString* out) const noexcept {
  return Protect([&] {
    const auto& value = State().document(key).metadata();
    if (field == DocumentMetadataField::Domain) {
      if (!value.domain().empty()) *out = value.domain();
      else {
        ruyijs::domain::browser_environment::UrlValue parsed(domain::to_utf8(url));
        *out = domain::from_utf8(parsed.hostname());
      }
    } else if (field == DocumentMetadataField::Direction) *out = value.direction();
    else *out = value.design_mode() ? u"on" : u"off";
  });
}
Status Model::WriteDocumentMetadata(NodeKey key, DocumentMetadataField field, DomStringView text) noexcept {
  return Protect([&] {
    auto& value = State().document(key).metadata();
    const auto kind = field == DocumentMetadataField::Domain ? domain::MetadataField::Domain
        : field == DocumentMetadataField::Direction ? domain::MetadataField::Direction : domain::MetadataField::DesignMode;
    if (kind == domain::MetadataField::Domain && text.empty()) return;
    if (kind != domain::MetadataField::DesignMode)
      State().budget(0, text.size(), value.replaced_units(kind));
    value.set(kind, text);
  });
}
bool SupportedEditCommand(DomStringView command) noexcept {
  return domain::supported_edit_command(command);
}
}
