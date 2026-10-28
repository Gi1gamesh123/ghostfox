#include "document_model/model_internal.h"
#include "domain/document_model/xml_parser.h"
#include "domain/document_model/xml_parsererror.h"

namespace ruyijs::document_model {
namespace {
// HTML kinds: the original inert tokenizer/tree-builder; scripts never run.
void ParseHtml(domain::DocumentAggregate& owner, DomStringView markup, const html::Budget& charge,
               const html::TemplateSink& templates) {
  auto session = std::make_unique<html::Session>(html::Normalize(markup));
  session->tree.root = owner.id();
  session->tree.template_sink = &templates;
  for (;;) {
    html::Token token = session->tokenizer.Next();
    const bool end = token.kind == html::Token::Kind::End;
    html::Build(owner, session->tree, token, charge);
    if (end) break;
  }
}
}  // namespace

Status Model::ParseDocument(ParseKind kind, DomStringView markup, Draft* result,
                            DomStringView location) noexcept {
  return Protect([&] {
    if (markup.size() > State().limits.text_units)
      throw domain::DomError(domain::DomErrorCode::LimitExceeded, "Markup exceeds the DOM text limit");
    // The two HTML kinds keep their original document flags unchanged; only
    // the XML kinds stop claiming HTML syntax now that they are parsed as XML.
    const bool html = kind == ParseKind::Html;
    const bool html_parser = html || kind == ParseKind::PublicHtml;
    DocumentId identity = State().allocate_document();
    auto fresh = [&] {
      auto document = std::make_unique<domain::DocumentAggregate>(1,
          html ? domain::DocumentMode::Html : domain::DocumentMode::Xml, false);
      document->set_ready_state(domain::ReadyState::Complete);
      document->set_url16(u"about:blank");
      document->metadata().set_parsed_html_syntax(kind == ParseKind::PublicHtml);
      document->metadata().set_xml_parsed(!html_parser);
      document->metadata().set_xml_media_type(kind == ParseKind::PublicHtml ? domain::XmlMediaType::Html
          : kind == ParseKind::TextXml ? domain::XmlMediaType::TextXml
          : kind == ParseKind::Svg ? domain::XmlMediaType::Svg
          : kind == ParseKind::Xhtml ? domain::XmlMediaType::Xhtml : domain::XmlMediaType::Xml);
      domain::DocumentAggregate& owner = *document;
      State().documents[identity] = std::move(document);
      return &owner;
    };
    domain::DocumentAggregate* owner = fresh();
    const html::Budget charge = [&](size_t nodes, size_t units) { State().budget(nodes, units); };
    try {
      if (html_parser) {
        html::TemplateSink sink;
        BindTemplateSink(sink, State(), identity);
        ParseHtml(*owner, markup, charge, sink);
      } else {
        // XML media types: the namespace-aware XML parser (D23 resolved). A
        // document that is not well-formed becomes Firefox's parsererror
        // document instead of a partial tree.
        domain::XmlParser parser(*owner, markup, charge);
        try {
          parser.Parse();
        } catch (const domain::XmlError& error) {
          owner = fresh();
          domain::BuildXmlParserError(*owner, parser.text(), error, location);
        }
      }
    } catch (...) {
      State().documents.erase(identity);
      throw;
    }
    *result = State().remember({identity, 1}, true);
  });
}
}  // namespace ruyijs::document_model
