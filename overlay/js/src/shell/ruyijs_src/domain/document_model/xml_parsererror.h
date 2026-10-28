#ifndef RUYIJS_DOMAIN_DOCUMENT_MODEL_XML_PARSERERROR_H
#define RUYIJS_DOMAIN_DOCUMENT_MODEL_XML_PARSERERROR_H
// Firefox 158's document for XML that is not well-formed (artifacts/xml-ns/
// more): the one root <parsererror xmlns="http://www.mozilla.org/newlayout/
// xml/parsererror.xml"> holding the text "XML Parsing Error: <message>\n
// Location: <url>\nLine Number <l>, Column <c>:" and a <sourcetext> child with
// the offending line, a newline and "-" * (c - 1) + "^"; the root has dir="ltr". Line and column are
// 1-based over the line-normalized input.
#include <string>

#include "domain/document_model/document.h"
#include "domain/document_model/xml_reader.h"

namespace ruyijs::domain::document_model {
inline void BuildXmlParserError(DocumentAggregate& doc, const std::u16string& text, const XmlError& error,
                                std::u16string_view location) {
  static constexpr char16_t kNs[] = u"http://www.mozilla.org/newlayout/xml/parsererror.xml";
  const size_t index = error.index < text.size() ? error.index : text.size();
  const size_t line_start = index ? text.rfind(u'\n', index - 1) + 1 : 0;  // npos + 1 == 0
  size_t line_end = text.find(u'\n', index);
  if (line_end == std::u16string::npos) line_end = text.size();
  size_t line = 1;
  for (size_t i = 0; i < line_start; ++i) line += text[i] == u'\n';
  const size_t column = index - line_start + 1;
  auto number = [](size_t value) {
    std::u16string out;
    const std::string digits = std::to_string(value);
    for (char c : digits) out.push_back(char16_t(c));
    return out;
  };
  const std::u16string message = u"XML Parsing Error: " + error.message + u"\nLocation: " +
      std::u16string(location) + u"\nLine Number " + number(line) + u", Column " + number(column) + u":";
  std::u16string source = text.substr(line_start, line_end - line_start) + u"\n";
  source.append(column - 1, u'-');
  source.push_back(u'^');
  const NodeId root = doc.create_element(TagName(u"parsererror", kNs, u"", false));
  const NodeId sourcetext = doc.create_element(TagName(u"sourcetext", kNs, u"", false));
  doc.get_element(root)->set_attribute_ns16(u"", u"dir", u"ltr");
  doc.append_child(root, doc.create_text_node16(message));
  doc.append_child(sourcetext, doc.create_text_node16(source));
  doc.append_child(root, sourcetext);
  doc.append_child(doc.id(), root);
}
}  // namespace ruyijs::domain::document_model
#endif
