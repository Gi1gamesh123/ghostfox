#pragma once
#include "domain/document_model/dom_string.h"

namespace ruyijs::domain::document_model {
enum class MetadataField { Domain, Direction, DesignMode };
enum class XmlMediaType { Xml, TextXml, Xhtml, Svg, Html };

// Owned by one DocumentAggregate. No HTML attribute or navigation state copy.
class DocumentMetadata {
 public:
  const DomString& domain() const noexcept { return domain_; }
  const DomString& direction() const noexcept { return direction_; }
  bool design_mode() const noexcept { return design_mode_; }
  XmlMediaType xml_media_type() const noexcept { return xml_media_type_; }
  void set_xml_media_type(XmlMediaType value) noexcept { xml_media_type_ = value; }
  // Built by the XML parser (DOMParser's XML media types): an XMLDocument,
  // always in no-quirks mode.
  bool xml_parsed() const noexcept { return xml_parsed_; }
  void set_xml_parsed(bool value) noexcept { xml_parsed_ = value; }
  bool parsed_html_syntax() const noexcept { return parsed_html_syntax_; }
  void set_parsed_html_syntax(bool value) noexcept { parsed_html_syntax_ = value; }
  DomStringView content_type(bool html) const noexcept {
    if (html || xml_media_type_ == XmlMediaType::Html) return u"text/html";
    return xml_media_type_ == XmlMediaType::Xhtml ? u"application/xhtml+xml"
        : xml_media_type_ == XmlMediaType::Svg ? u"image/svg+xml"
        : xml_media_type_ == XmlMediaType::TextXml ? u"text/xml" : u"application/xml";
  }
  size_t storage_units() const noexcept { return domain_.size() + direction_.size(); }
  size_t replaced_units(MetadataField field) const noexcept {
    return field == MetadataField::Domain ? domain_.size()
        : field == MetadataField::Direction ? direction_.size() : 0;
  }
  void set(MetadataField field, DomStringView value) {
    if (field == MetadataField::Domain) {
      if (!value.empty()) domain_ = value;
    } else if (field == MetadataField::Direction) direction_ = value;
    else design_mode_ = value == u"on";
  }
 private:
  DomString domain_, direction_;
  bool design_mode_ = false;
  XmlMediaType xml_media_type_ = XmlMediaType::Xml;
  bool parsed_html_syntax_ = false;
  bool xml_parsed_ = false;
};
inline bool supported_edit_command(DomStringView command) {
  for (const auto* name : {u"bold", u"italic", u"underline", u"cut", u"copy", u"paste", u"delete"})
    if (command == name) return true;
  return false;
}
}
