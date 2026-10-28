// Engine-independent HTML tokenizer for the Document Model parser.
// It emits UTF-16 tokens with ordered, first-wins attributes. Tree construction
// lives in model_tree_builder.cpp and writes only into the DocumentAggregate owner.
// Simplifications against the WHATWG tokenizer are listed in docs/DEVIATIONS.md D7.
#pragma once
#include "document_model/model.h"
#include <initializer_list>
#include <utility>
#include <vector>

namespace ruyijs::document_model::html {
struct Attribute { DomString name, value; };
struct Token {
  enum class Kind { StartTag, EndTag, Text, Comment, Doctype, End };
  Kind kind = Kind::End;
  DomString name, data;  // name: lowercase tag name; data: text or comment.
  std::vector<Attribute> attributes;
  bool self_closing = false;
  size_t offset = 0;  // Text: position of its first character in the input.
  DomString public_id, system_id;
  bool doctype_cloneable = true;
};
// Input stream preprocessing: CRLF and lone CR become LF.
DomString Normalize(DomStringView input);
// Content model the tokenizer enters after a start tag, or at fragment start.
enum class Content { Data, RcData, RawText, ScriptData, PlainText };
Content ContentFor(DomStringView tag) noexcept;

class Tokenizer {
 public:
  // The view must outlive the tokenizer; the parser session owns the copy.
  Tokenizer(DomStringView input, Content initial = Content::Data, DomStringView context = {});
  // After a raw-text start tag the following Text token holds the raw content
  // and an EndTag closes it. Raw text cut by end of input yields no EndTag.
  Token Next();
  // document.write: the owning session grew its input at or after the current
  // position; the tokenizer keeps its position and state over the new view.
  void Rebase(DomStringView input) noexcept { input_ = input; }
  // The tree builder's "adjusted current node is foreign" flag: start tags then
  // keep the data state and CDATA sections are text.
  void Follow(const bool* foreign) noexcept { foreign_ = foreign; }
  size_t Position() const noexcept { return pos_; }
 private:
  bool Peek(char16_t) const noexcept;
  bool StartsWithFolded(const char16_t* literal) const noexcept;
  Token Text();
  Token Raw();
  Token Markup();
  Token Doctype();
  Token Bogus();
  Token Tag(bool end);
  bool Attributes(Token& token);  // false: input ended inside the tag.
  DomString Value(char16_t quote);
  void Reference(DomString& out, bool attribute);
  DomStringView input_;
  size_t pos_ = 0;
  Content content_;
  DomString raw_end_;  // Tag name that ends the current raw content.
  bool pending_end_ = false;
  const bool* foreign_ = nullptr;
  bool Foreign() const noexcept { return foreign_ && *foreign_; }
};
// Appends the decoded character reference that starts at input[pos] ('&').
// Returns the consumed length, or 0 when the text stays literal.
size_t DecodeReference(DomStringView input, size_t pos, bool attribute, DomString& out);

// Element categories (model_html_categories.cpp), HTML local names only.
bool In(DomStringView name, std::initializer_list<const char16_t*> names);
bool HeadVoid(DomStringView name);     // base, basefont, bgsound, link, meta
bool HeadContent(DomStringView name);  // also processed by the "in head" rules
bool ClosesP(DomStringView name);      // start tags that close an open <p>
bool Heading(DomStringView name);
bool Implied(DomStringView name);      // generate-implied-end-tags set
bool Boundary(DomStringView name);     // default scope boundaries
bool Special(DomStringView name);      // the "special" category

// Foreign content (model_tree_foreign.cpp): SVG and MathML inside HTML.
enum class TagSpace { Html, Svg, MathMl };
DomStringView SpaceUri(TagSpace) noexcept;
DomString ForeignTagName(TagSpace, DomStringView lower);  // SVG camel case
// The adjusted attribute name and its namespace (empty: no namespace).
void ForeignAttribute(TagSpace, DomStringView lower, DomString* qualified, DomStringView* namespace_uri);
bool BreaksOutOfForeign(const struct Token&);
bool HtmlIntegrationPoint(TagSpace, DomStringView local);

// Real-change sink for a document load. The document bridge fills these from
// the owner's bounded notice log, so nodes the parser inserts are observable
// exactly like DOM-API changes. Fragment parsing leaves the pointer null: its
// root is unpublished and can never be observed.
struct ParserNotice {
  void* context = nullptr;
  uint64_t document = 0;
  void (*inserted)(void*, uint64_t, uint64_t, const std::vector<uint64_t>&) = nullptr;
  void (*attributes)(void*, uint64_t, uint64_t, DomStringView, const DomString*) = nullptr;
  void (*data)(void*, uint64_t, uint64_t, const DomString*) = nullptr;
};

// Template contents: when a template element leaves the stack of open elements
// (or input ends) its children move into its content fragment, which lives in
// the inert template document only the Model owner can reach.
struct TemplateSink {
  void* context = nullptr;
  uint64_t document = 0;
  void (*closed)(void*, uint64_t, uint64_t) = nullptr;
};

// The table modes follow AfterBody so `mode >= InBody` still means "past the head".
enum class Mode { BeforeHtml, BeforeHead, InHead, AfterHead, InBody, AfterBody,
                  InTable, InTableText, InCaption, InColumnGroup, InTableBody, InRow, InCell };
// Tree-construction position. Ids name nodes of the one DocumentAggregate; the
// open stack never owns them, so script mutations between steps stay visible.
struct TreeState {
  Mode mode = Mode::BeforeHtml;
  uint64_t root = 0;  // Parent of top-level nodes: the Document or fragment root.
  uint64_t html = 0, head = 0, body = 0;
  std::vector<uint64_t> open;
  bool raw = false;           // Inside script/style/title... raw content.
  bool skip_newline = false;  // First LF after <pre>, <listing>, <textarea>.
  bool scripts = false;       // Document load pauses at parser-inserted scripts.
  // Set by the bridge for a document load; null in the fragment path.
  const ParserNotice* notice = nullptr;
  // Foreign content: the tokenizer's flag, and a fragment context's namespace.
  bool foreign = false;
  TagSpace context_space = TagSpace::Html;
  // A fragment's context element local name (HTML context), for "reset the
  // insertion mode appropriately"; empty for a document load.
  DomString context;
  // Table parsing: foster parenting while "in table" hands a token to the body
  // rules, and the pending character tokens of "in table text".
  bool foster = false;
  DomString pending_text;
  Mode original = Mode::InTable;
  // The list of active formatting elements: the element and the token it was
  // created for (to recreate it); id 0 is a marker.
  struct Formatting { uint64_t id = 0; Token token; };
  std::vector<Formatting> formatting;
  // Open template elements, outermost first, and where their children go.
  std::vector<uint64_t> templates;
  const TemplateSink* template_sink = nullptr;
  // The document load's input, for script source positions; empty for a
  // fragment. The cursor counts lines forward only, once per document.
  DomStringView source;
  size_t cursor = 0, line_start = 0;
  uint32_t line = 1;
  // Ranges of |source| that document.write inserted: they do not move the
  // line/column of the document's own scripts.
  std::vector<std::pair<size_t, size_t>> inserted;
};
// One document load: owns the normalized markup the tokenizer reads.
struct Session {
  explicit Session(DomString markup) : input(std::move(markup)), tokenizer(input) {
    tree.source = input;
    tokenizer.Follow(&tree.foreign);
  }
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  DomString input;
  Tokenizer tokenizer;
  // The script-created insertion point: set while the parser is paused at a
  // parser-inserted script it just closed, unset once parsing resumes.
  static constexpr size_t kNoInsertion = size_t(-1);
  size_t insertion = kNoInsertion;
  // A script the written text closed while document.write parsed it; the
  // parser returns it before reading further (it runs after the writer).
  uint64_t pending_script = 0;
  // document.open(): input comes only from document.write; close() ends it.
  bool script_created = false;
  // Inserts |text| at the insertion point and advances it (HTML "document.write").
  void Write(DomStringView text) {
    input.insert(insertion, text.data(), text.size());
    bool merged = false;  // Text written inside earlier written text joins that range.
    for (auto& range : tree.inserted) {
      if (range.first >= insertion) range.first += text.size();
      else if (range.first + range.second > insertion) { range.second += text.size(); merged = true; }
    }
    if (!merged) tree.inserted.emplace_back(insertion, text.size());
    insertion += text.size();
    tokenizer.Rebase(input);
    tree.source = input;
  }
  // The parser's change sink; stable because the session is heap-owned.
  ParserNotice notice;
  TemplateSink templates;
  TreeState tree;
};
}
