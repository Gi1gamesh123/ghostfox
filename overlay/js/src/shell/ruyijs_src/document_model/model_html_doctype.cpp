// Doctype token values from the existing input cursor. No second parser state.
#include "document_model/model_html.h"
namespace ruyijs::document_model::html {
namespace {
bool Space(char16_t c) { return c == u' ' || c == u'\t' || c == u'\n' || c == u'\f'; }
char16_t Lower(char16_t c) { return c >= u'A' && c <= u'Z' ? char16_t(c + 32) : c; }
}
Token Tokenizer::Doctype() {
  Token token; token.kind = Token::Kind::Doctype;
  pos_ += 7;
  const size_t close = input_.find(u'>', pos_);
  const size_t end = close == DomStringView::npos ? input_.size() : close;
  DomString body;
  for (; pos_ < end; ++pos_) if (input_[pos_]) body += input_[pos_];
  if (close != DomStringView::npos) ++pos_;
  size_t at = 0;
  const auto space = [&] { while (at < body.size() && Space(body[at])) ++at; };
  space();
  while (at < body.size() && !Space(body[at])) token.name += Lower(body[at++]);
  space();
  const auto keyword = [&](const char16_t* name) {
    for (size_t i = 0; name[i]; ++i)
      if (at + i >= body.size() || Lower(body[at + i]) != name[i]) return false;
    return true;
  };
  const bool public_id = keyword(u"public"), system_id = keyword(u"system");
  if (!public_id && !system_id) return token;
  at += 6; space();
  const auto quoted = [&](DomString& text) {
    if (at == body.size() || (body[at] != u'\'' && body[at] != u'"')) return false;
    const char16_t quote = body[at++];
    while (at < body.size() && body[at] != quote) text += body[at++];
    if (at < body.size()) ++at;
    space(); return true;
  };
  if (public_id) {
    token.doctype_cloneable = quoted(token.public_id);
    if (token.doctype_cloneable) quoted(token.system_id);
  } else {
    quoted(token.system_id); token.doctype_cloneable = false;
  }
  return token;
}
}
