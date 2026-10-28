#ifndef RUYIJS_DOMAIN_DOCUMENT_MODEL_XML_MARKUP_H
#define RUYIJS_DOMAIN_DOCUMENT_MODEL_XML_MARKUP_H
// Markup pieces of the XML parser (xml_parser.h): entity references, attribute
// values and the DOCTYPE with its internal-subset general entities. Firefox
// 158 behaviour (artifacts/xml-ns/more): an internal `<!ENTITY e "v">` expands
// in content, an unknown name is "undefined entity" at its '&', a literal tab
// or newline in an attribute value becomes a space while a character
// reference keeps its character.
#include <map>
#include <string>

#include "domain/document_model/xml_reader.h"

namespace ruyijs::domain::document_model {
struct XmlDoctype {
  bool present = false;
  std::u16string name, public_id, system_id;
};
class XmlMarkup {
 public:
  explicit XmlMarkup(XmlReader& reader) : r_(reader) {}
  // A reference at pos ('&') appended to `out`; `attribute` applies value
  // normalization to the replacement text of an internal entity.
  void Reference(std::u16string* out, bool attribute) {
    if (r_.peek(1) == u'#') return r_.CharacterReference(out);
    const size_t at = r_.pos;
    ++r_.pos;
    if (r_.done() || !XmlReader::NameStart(r_.peek())) XmlReader::Fail(u"not well-formed", r_.pos);
    const std::u16string name = r_.Name();
    if (r_.peek() != u';') XmlReader::Fail(u"not well-formed", r_.pos);
    ++r_.pos;
    if (name == u"lt") out->push_back(u'<');
    else if (name == u"gt") out->push_back(u'>');
    else if (name == u"amp") out->push_back(u'&');
    else if (name == u"apos") out->push_back(u'\'');
    else if (name == u"quot") out->push_back(u'"');
    else {
      auto found = entities_.find(name);
      if (found == entities_.end()) XmlReader::Fail(u"undefined entity", at);
      for (char16_t c : found->second) out->push_back(attribute && XmlReader::Space(c) ? u' ' : c);
    }
  }
  // A quoted attribute value; pos is on the opening quote.
  std::u16string AttributeValue() {
    const char16_t quote = r_.peek();
    if (quote != u'"' && quote != u'\'') XmlReader::Fail(u"not well-formed", r_.pos);
    ++r_.pos;
    std::u16string value;
    for (;;) {
      if (r_.done()) XmlReader::Fail(u"unclosed token", r_.pos);
      const char16_t c = r_.peek();
      if (c == quote) { ++r_.pos; return value; }
      if (c == u'<') XmlReader::Fail(u"not well-formed", r_.pos);
      if (c == u'&') { Reference(&value, true); continue; }
      value.push_back(XmlReader::Space(c) ? u' ' : c);
      ++r_.pos;
    }
  }
  // `<!DOCTYPE name [PUBLIC "p" "s" | SYSTEM "s"] [internal subset]>`; pos on '<'.
  XmlDoctype Doctype() {
    XmlDoctype doctype;
    doctype.present = true;
    r_.pos += 9;
    if (!r_.SkipSpace()) XmlReader::Fail(u"syntax error", r_.pos);
    doctype.name = r_.Name();
    r_.SkipSpace();
    if (r_.starts(u"PUBLIC")) {
      r_.pos += 6;
      r_.SkipSpace();
      doctype.public_id = Literal();
      r_.SkipSpace();
      doctype.system_id = Literal();
    } else if (r_.starts(u"SYSTEM")) {
      r_.pos += 6;
      r_.SkipSpace();
      doctype.system_id = Literal();
    }
    r_.SkipSpace();
    if (r_.peek() == u'[') Subset();
    r_.SkipSpace();
    if (r_.peek() != u'>') XmlReader::Fail(u"syntax error", r_.pos);
    ++r_.pos;
    return doctype;
  }

 private:
  std::u16string Literal() {
    const char16_t quote = r_.peek();
    if (quote != u'"' && quote != u'\'') XmlReader::Fail(u"syntax error", r_.pos);
    const size_t start = ++r_.pos;
    while (!r_.done() && r_.peek() != quote) ++r_.pos;
    if (r_.done()) XmlReader::Fail(u"unclosed token", start - 1);
    return r_.text().substr(start, r_.pos++ - start);
  }
  // Internal subset: general entities with a literal value are kept (the first
  // declaration wins); every other declaration, comment or PI is skipped.
  void Subset() {
    ++r_.pos;
    for (;;) {
      r_.SkipSpace();
      if (r_.done()) XmlReader::Fail(u"unclosed token", r_.pos);
      if (r_.peek() == u']') { ++r_.pos; return; }
      if (r_.starts(u"<!ENTITY")) {
        r_.pos += 8;
        r_.SkipSpace();
        const bool parameter = r_.peek() == u'%';
        if (parameter) { ++r_.pos; r_.SkipSpace(); }
        const std::u16string name = r_.Name();
        r_.SkipSpace();
        if (r_.peek() == u'"' || r_.peek() == u'\'') {
          std::u16string value = Literal();
          if (!parameter) entities_.emplace(name, std::move(value));
        }
      }
      Skip();
    }
  }
  // To the '>' closing the current declaration, honouring quotes.
  void Skip() {
    if (r_.starts(u"<!--")) {
      const size_t end = r_.text().find(u"-->", r_.pos + 4);
      if (end == std::u16string::npos) XmlReader::Fail(u"unclosed token", r_.pos);
      r_.pos = end + 3;
      return;
    }
    char16_t quote = 0;
    for (; !r_.done(); ++r_.pos) {
      const char16_t c = r_.peek();
      if (quote) { if (c == quote) quote = 0; continue; }
      if (c == u'"' || c == u'\'') quote = c;
      else if (c == u'>') { ++r_.pos; return; }
    }
    XmlReader::Fail(u"unclosed token", r_.pos);
  }
  XmlReader& r_;
  std::map<std::u16string, std::u16string> entities_;
};
}  // namespace ruyijs::domain::document_model
#endif
