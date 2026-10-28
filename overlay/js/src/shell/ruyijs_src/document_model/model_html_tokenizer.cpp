#include "document_model/model_html.h"

namespace ruyijs::document_model::html {
namespace {
bool Space(char16_t c) { return c == u'\t' || c == u'\n' || c == u'\f' || c == u' '; }
bool Alpha(char16_t c) { return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z'); }
char16_t Lower(char16_t c) { return c >= u'A' && c <= u'Z' ? char16_t(c + 32) : c == 0 ? u'\xFFFD' : c; }
}
DomString Normalize(DomStringView input) {
  // Input stream preprocessing: CRLF and lone CR become LF.
  DomString out;
  out.reserve(input.size());
  for (size_t i = 0; i < input.size(); ++i) {
    if (input[i] != u'\r') { out += input[i]; continue; }
    out += u'\n';
    if (i + 1 < input.size() && input[i + 1] == u'\n') ++i;
  }
  return out;
}
Content ContentFor(DomStringView tag) noexcept {
  if (tag == u"script") return Content::ScriptData;
  if (tag == u"textarea" || tag == u"title") return Content::RcData;
  // noscript is raw text because page realms always have scripting enabled.
  if (tag == u"style" || tag == u"xmp" || tag == u"iframe" || tag == u"noembed" ||
      tag == u"noframes" || tag == u"noscript") return Content::RawText;
  return tag == u"plaintext" ? Content::PlainText : Content::Data;
}
Tokenizer::Tokenizer(DomStringView input, Content initial, DomStringView context)
    : input_(input), content_(initial), raw_end_(context) {}
bool Tokenizer::Peek(char16_t c) const noexcept { return pos_ < input_.size() && input_[pos_] == c; }
bool Tokenizer::StartsWithFolded(const char16_t* literal) const noexcept {
  for (size_t i = 0; literal[i]; ++i)
    if (pos_ + i >= input_.size() || Lower(input_[pos_ + i]) != literal[i]) return false;
  return true;
}
Token Tokenizer::Next() {
  if (pending_end_) {
    Token token;
    token.kind = Token::Kind::EndTag;
    token.name.swap(raw_end_);
    pending_end_ = false;
    content_ = Content::Data;
    return token;
  }
  if (content_ != Content::Data) return Raw();
  if (pos_ >= input_.size()) return {};
  if (input_[pos_] != u'<') return Text();
  const size_t size = input_.size();
  char16_t next = pos_ + 1 < size ? input_[pos_ + 1] : 0;
  if (next == u'!') { pos_ += 2; return Markup(); }
  if (Alpha(next)) { pos_ += 1; return Tag(false); }
  if (next == u'/') {
    char16_t after = pos_ + 2 < size ? input_[pos_ + 2] : 0;
    if (Alpha(after)) { pos_ += 2; return Tag(true); }
    if (after == u'>') { pos_ += 3; return Next(); }
    if (pos_ + 2 < size) { pos_ += 2; return Bogus(); }
  } else if (next == u'?') {
    pos_ += 1;
    return Bogus();
  }
  Token token;  // A '<' that opens no tag is text.
  token.kind = Token::Kind::Text;
  token.data = pos_ + 2 >= size && next == u'/' ? u"</" : u"<";
  pos_ += token.data.size();
  return token;
}
Token Tokenizer::Text() {
  Token token;
  token.kind = Token::Kind::Text;
  token.offset = pos_;
  while (pos_ < input_.size() && input_[pos_] != u'<') {
    char16_t c = input_[pos_];
    if (c == u'&') {
      size_t used = DecodeReference(input_, pos_, false, token.data);
      if (used) { pos_ += used; continue; }
    }
    if (c) token.data += c;  // In-body tree construction ignores NUL.
    ++pos_;
  }
  return token;
}
Token Tokenizer::Raw() {
  const size_t size = input_.size();
  size_t found = DomStringView::npos;
  // An empty end name (fragment context) never matches: the rest is text.
  for (size_t at = pos_; content_ != Content::PlainText && !raw_end_.empty() &&
                         (at = input_.find(u"</", at)) != DomStringView::npos; at += 2) {
    size_t name = at + 2, k = 0;
    while (k < raw_end_.size() && name + k < size && Lower(input_[name + k]) == raw_end_[k]) ++k;
    if (k == raw_end_.size() && name + k < size &&
        (Space(input_[name + k]) || input_[name + k] == u'/' || input_[name + k] == u'>')) {
      found = at;
      break;
    }
  }
  const size_t stop = found == DomStringView::npos ? size : found;
  Token token;
  token.kind = Token::Kind::Text;
  token.offset = pos_;
  for (size_t i = pos_; i < stop; ++i) {
    if (content_ == Content::RcData && input_[i] == u'&') {
      size_t used = DecodeReference(input_.substr(0, stop), i, false, token.data);
      if (used) { i += used - 1; continue; }
    }
    token.data += input_[i] ? input_[i] : u'\xFFFD';
  }
  if (found == DomStringView::npos) {
    pos_ = size;  // Raw content cut by end of input leaves the element open.
    content_ = Content::Data;
    raw_end_.clear();
  } else {
    size_t close = input_.find(u'>', found);
    pos_ = close == DomStringView::npos ? size : close + 1;
    pending_end_ = true;
  }
  return token.data.empty() ? Next() : token;
}
Token Tokenizer::Markup() {
  Token token;
  token.kind = Token::Kind::Comment;
  if (StartsWithFolded(u"--")) {
    pos_ += 2;
    if (Peek(u'>')) { ++pos_; return token; }
    if (StartsWithFolded(u"->")) { pos_ += 2; return token; }
    size_t plain = input_.find(u"-->", pos_), bang = input_.find(u"--!>", pos_);
    size_t end = plain < bang ? plain : bang;
    token.data.assign(input_.substr(pos_, end == DomStringView::npos ? DomStringView::npos : end - pos_));
    pos_ = end == DomStringView::npos ? input_.size() : end + (end == plain ? 3 : 4);
    return token;
  }
  if (StartsWithFolded(u"doctype")) return Doctype();
  if (Foreign() && input_.substr(pos_, 7) == DomStringView(u"[CDATA[")) {  // Text in foreign content.
    pos_ += 7;
    const size_t end = input_.find(u"]]>", pos_);
    token.kind = Token::Kind::Text;
    token.offset = pos_;
    token.data.assign(input_.substr(pos_, end == DomStringView::npos ? DomStringView::npos : end - pos_));
    pos_ = end == DomStringView::npos ? input_.size() : end + 3;
    return token.data.empty() ? Next() : token;
  }
  return Bogus();  // Includes CDATA sections outside foreign content.
}
Token Tokenizer::Bogus() {
  Token token;
  token.kind = Token::Kind::Comment;
  size_t close = input_.find(u'>', pos_);
  token.data.assign(input_.substr(pos_, close == DomStringView::npos ? DomStringView::npos : close - pos_));
  pos_ = close == DomStringView::npos ? input_.size() : close + 1;
  return token;
}
Token Tokenizer::Tag(bool end) {
  Token token;
  token.kind = end ? Token::Kind::EndTag : Token::Kind::StartTag;
  while (pos_ < input_.size() && !Space(input_[pos_]) && input_[pos_] != u'/' && input_[pos_] != u'>')
    token.name += Lower(input_[pos_++]);
  if (!Attributes(token)) return {};  // End of input inside a tag drops it.
  if (end) {
    token.attributes.clear();
    token.self_closing = false;
  } else if (!Foreign() && (content_ = ContentFor(token.name)) != Content::Data) {
    raw_end_ = token.name;
  }
  return token;
}
bool Tokenizer::Attributes(Token& token) {
  while (pos_ < input_.size()) {
    char16_t c = input_[pos_];
    if (Space(c)) { ++pos_; continue; }
    if (c == u'>') { ++pos_; return true; }
    if (c == u'/') {
      ++pos_;
      if (Peek(u'>')) { token.self_closing = true; ++pos_; return true; }
      continue;
    }
    DomString name(1, Lower(c));  // A leading '=' belongs to the name.
    for (++pos_; pos_ < input_.size(); ++pos_) {
      c = input_[pos_];
      if (Space(c) || c == u'/' || c == u'>' || c == u'=') break;
      name += Lower(c);
    }
    while (pos_ < input_.size() && Space(input_[pos_])) ++pos_;
    DomString value;
    if (Peek(u'=')) {
      for (++pos_; pos_ < input_.size() && Space(input_[pos_]);) ++pos_;
      char16_t quote = Peek(u'"') || Peek(u'\'') ? input_[pos_++] : 0;
      value = Value(quote);
    }
    bool duplicate = false;  // The first occurrence of a name wins.
    for (const auto& attribute : token.attributes) duplicate = duplicate || attribute.name == name;
    if (!duplicate) token.attributes.push_back({std::move(name), std::move(value)});
  }
  return false;
}
DomString Tokenizer::Value(char16_t quote) {
  DomString out;
  while (pos_ < input_.size()) {
    char16_t c = input_[pos_];
    if (quote ? c == quote : (Space(c) || c == u'>')) {
      if (quote) ++pos_;
      return out;
    }
    if (c == u'&') {
      size_t used = DecodeReference(input_, pos_, true, out);
      if (used) { pos_ += used; continue; }
    }
    out += c ? c : u'\xFFFD';
    ++pos_;
  }
  return out;
}
}
