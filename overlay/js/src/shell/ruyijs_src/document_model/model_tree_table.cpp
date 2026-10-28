// The HTML table insertion modes (WHATWG 13.2.6.4.9-15), foster parenting,
// "reset the insertion mode appropriately" and the body rules for table and
// select tags (the relaxed select parsing Firefox 158 implements).
#include "document_model/model_tree_builder.h"

namespace ruyijs::document_model::html::detail {
namespace {
bool TableContext(DomStringView n) { return In(n, {u"table", u"tbody", u"tfoot", u"thead", u"tr"}); }
bool Section(DomStringView n) { return In(n, {u"tbody", u"tfoot", u"thead"}); }
bool Cell(DomStringView n) { return n == u"td" || n == u"th"; }
// "has an element in table scope" for one (possibly runtime) name.
bool Scoped(const Builder& b, DomStringView target) {
  for (auto it = b.tree.open.rbegin(); it != b.tree.open.rend(); ++it) {
    const DomStringView name = b.Name(*it);
    if (name == target) return true;
    if (In(name, {u"html", u"table", u"template"})) return false;
  }
  return false;
}
void Pop(Builder& b) { if (!b.tree.open.empty()) b.tree.open.pop_back(); }
void ImpliedEnds(Builder& b, DomStringView except = {}) {
  while (!b.tree.open.empty() && Implied(b.Name(b.Current())) && b.Name(b.Current()) != except) Pop(b);
}
void Start(Builder& b, const char16_t* name) { Token token{Kind::StartTag, DomString(name)}; b.Insert(token, true); }
void CloseCell(Builder& b) {
  ImpliedEnds(b);
  while (!b.tree.open.empty()) { const bool cell = Cell(b.Name(b.Current())); Pop(b); if (cell) break; }
  b.ClearToMarker();
  b.tree.mode = Mode::InRow;
}
bool InTable(Builder& b, Token& t) {
  TreeState& tree = b.tree;
  const DomStringView name = t.name;
  const bool start = t.kind == Kind::StartTag, end = t.kind == Kind::EndTag;
  if (t.kind == Kind::Text && In(b.Name(b.Current()), {u"table", u"tbody", u"template", u"tfoot", u"thead", u"tr"})) {
    tree.pending_text.clear(); tree.original = tree.mode; tree.mode = Mode::InTableText;
    return false;
  }
  if (t.kind == Kind::Comment) { b.Comment(t, b.Current()); return true; }
  if (t.kind == Kind::Doctype) return true;
  if (start) {
    if (name == u"caption" || name == u"colgroup") {
      b.ClearBackTo({u"table"}); b.Insert(t, true);
      if (name == u"caption") b.Marker();
      tree.mode = name == u"caption" ? Mode::InCaption : Mode::InColumnGroup;
      return true;
    }
    if (name == u"col") { b.ClearBackTo({u"table"}); Start(b, u"colgroup"); tree.mode = Mode::InColumnGroup; return false; }
    if (Section(name)) { b.ClearBackTo({u"table"}); b.Insert(t, true); tree.mode = Mode::InTableBody; return true; }
    if (Cell(name) || name == u"tr") { b.ClearBackTo({u"table"}); Start(b, u"tbody"); tree.mode = Mode::InTableBody; return false; }
    if (name == u"table") {
      if (!Scoped(b, u"table")) return true;
      b.PopUntil(u"table"); b.ResetMode();
      return false;
    }
    if (In(name, {u"style", u"script", u"template"})) { b.Insert(t, true); return true; }
    if (name == u"input") {
      for (const auto& attribute : t.attributes)
        if (attribute.name == u"type" && domain::ascii_lower(attribute.value) == u"hidden") { b.Insert(t, false); return true; }
    }
    if (name == u"form") { if (tree.templates.empty()) b.Insert(t, false); return true; }
  }
  if (end) {
    if (name == u"table") {
      if (Scoped(b, u"table")) { b.PopUntil(u"table"); b.ResetMode(); }
      return true;
    }
    if (In(name, {u"body", u"caption", u"col", u"colgroup", u"html", u"tbody", u"td", u"tfoot", u"th", u"thead", u"tr"}))
      return true;
    if (name == u"template") { b.EndOther(name); b.ResetMode(); return true; }
  }
  tree.foster = true; b.InBody(t); tree.foster = false;  // anything else: foster parenting
  return true;
}
bool InTableText(Builder& b, Token& t) {
  TreeState& tree = b.tree;
  if (t.kind == Kind::Text) {
    for (char16_t c : t.data) if (c) tree.pending_text.push_back(c);
    return true;
  }
  DomString pending;
  pending.swap(tree.pending_text);
  bool space = true;
  for (char16_t c : pending) space = space && Space(c);
  if (!pending.empty()) {
    tree.foster = !space;
    if (!space) b.Reconstruct();  // non-space table text takes the body rules, fostered
    b.Text(pending);
    tree.foster = false;
  }
  tree.mode = tree.original;
  return false;
}
bool InCaption(Builder& b, Token& t) {
  const DomStringView name = t.name;
  const bool start = t.kind == Kind::StartTag, end = t.kind == Kind::EndTag;
  const bool closes = (end && name == u"caption") ||
      (start && In(name, {u"caption", u"col", u"colgroup", u"tbody", u"td", u"tfoot", u"th", u"thead", u"tr"})) ||
      (end && name == u"table");
  if (closes) {
    if (!Scoped(b, u"caption")) return true;
    ImpliedEnds(b); b.PopUntil(u"caption"); b.ClearToMarker(); b.tree.mode = Mode::InTable;
    return end && name == u"caption";
  }
  if (end && In(name, {u"body", u"col", u"colgroup", u"html", u"tbody", u"td", u"tfoot", u"th", u"thead", u"tr"})) return true;
  b.InBody(t);
  return true;
}
bool InColumnGroup(Builder& b, Token& t) {
  const DomStringView name = t.name;
  const bool start = t.kind == Kind::StartTag, end = t.kind == Kind::EndTag;
  if (t.kind == Kind::Text && !b.Leading(t, true)) return true;
  if (t.kind == Kind::Comment) { b.Comment(t, b.Current()); return true; }
  if (t.kind == Kind::Doctype) return true;
  if (start && name == u"html") { b.InBody(t); return true; }
  if (start && name == u"col") { b.Insert(t, false); return true; }
  if (end && name == u"col") return true;
  if (name == u"template" && (start || end)) {
    if (start) b.Insert(t, true); else { b.EndOther(name); b.ResetMode(); }
    return true;
  }
  if (b.Name(b.Current()) != u"colgroup") return true;
  Pop(b); b.tree.mode = Mode::InTable;
  return end && name == u"colgroup";
}
bool InTableBody(Builder& b, Token& t) {
  const DomStringView name = t.name;
  const bool start = t.kind == Kind::StartTag, end = t.kind == Kind::EndTag;
  if (start && name == u"tr") { b.ClearBackTo({u"tbody", u"tfoot", u"thead"}); b.Insert(t, true); b.tree.mode = Mode::InRow; return true; }
  if (start && Cell(name)) {
    b.ClearBackTo({u"tbody", u"tfoot", u"thead"}); Start(b, u"tr"); b.tree.mode = Mode::InRow;
    return false;
  }
  if (end && Section(name)) {
    if (!Scoped(b, name)) return true;
    b.ClearBackTo({u"tbody", u"tfoot", u"thead"}); Pop(b); b.tree.mode = Mode::InTable;
    return true;
  }
  if ((start && In(name, {u"caption", u"col", u"colgroup", u"tbody", u"tfoot", u"thead"})) || (end && name == u"table")) {
    if (!Scoped(b, u"tbody") && !Scoped(b, u"thead") && !Scoped(b, u"tfoot")) return true;
    b.ClearBackTo({u"tbody", u"tfoot", u"thead"}); Pop(b); b.tree.mode = Mode::InTable;
    return false;
  }
  if (end && In(name, {u"body", u"caption", u"col", u"colgroup", u"html", u"td", u"th", u"tr"})) return true;
  return InTable(b, t);
}
bool InRow(Builder& b, Token& t) {
  const DomStringView name = t.name;
  const bool start = t.kind == Kind::StartTag, end = t.kind == Kind::EndTag;
  if (start && Cell(name)) { b.ClearBackTo({u"tr"}); b.Insert(t, true); b.Marker(); b.tree.mode = Mode::InCell; return true; }
  const bool closeRow = (end && name == u"tr") ||
      (start && In(name, {u"caption", u"col", u"colgroup", u"tbody", u"tfoot", u"thead", u"tr"})) || (end && name == u"table") ||
      (end && Section(name) && Scoped(b, name));
  if (closeRow) {
    if (!Scoped(b, u"tr")) return true;
    b.ClearBackTo({u"tr"}); Pop(b); b.tree.mode = Mode::InTableBody;
    return end && name == u"tr";
  }
  if (end && In(name, {u"body", u"caption", u"col", u"colgroup", u"html", u"td", u"th", u"tbody", u"tfoot", u"thead"})) return true;
  return InTable(b, t);
}
bool InCell(Builder& b, Token& t) {
  const DomStringView name = t.name;
  const bool start = t.kind == Kind::StartTag, end = t.kind == Kind::EndTag;
  if (end && Cell(name)) {
    if (!Scoped(b, name)) return true;
    ImpliedEnds(b); b.PopUntil(name); b.ClearToMarker(); b.tree.mode = Mode::InRow;
    return true;
  }
  if (start && In(name, {u"caption", u"col", u"colgroup", u"tbody", u"td", u"tfoot", u"th", u"thead", u"tr"})) {
    if (!Scoped(b, u"td") && !Scoped(b, u"th")) return true;
    CloseCell(b);
    return false;
  }
  if (end && In(name, {u"body", u"caption", u"col", u"colgroup", u"html"})) return true;
  if (end && In(name, {u"table", u"tbody", u"tfoot", u"thead", u"tr"})) {
    if (!Scoped(b, name)) return true;
    CloseCell(b);
    return false;
  }
  b.InBody(t);
  return true;
}
}  // namespace

void Builder::LocationFor(NodeId target, NodeId* parent, NodeId* before) const {
  *parent = target;
  *before = 0;
  if (!tree.foster || !TableContext(Name(*parent))) return;
  size_t table = tree.open.size(), templ = tree.open.size();
  for (size_t i = 0; i < tree.open.size(); ++i) {
    if (Name(tree.open[i]) == u"table") table = i;
    if (Name(tree.open[i]) == u"template") templ = i;
  }
  if (templ < tree.open.size() && (table == tree.open.size() || templ > table)) { *parent = tree.open[templ]; return; }
  if (table == tree.open.size()) { *parent = tree.open.front(); return; }
  const NodeId element = tree.open[table];
  if (const NodeId owner = doc.require(element).parent_id()) { *parent = owner; *before = element; return; }
  *parent = table ? tree.open[table - 1] : tree.root;
}
bool Builder::InTableScope(std::initializer_list<const char16_t*> names) const {
  for (const char16_t* name : names) if (Scoped(*this, name)) return true;
  return false;
}
void Builder::ClearBackTo(std::initializer_list<const char16_t*> names) {
  while (!tree.open.empty()) {
    const DomStringView name = Name(Current());
    if (name == u"html" || name == u"template" || In(name, names)) return;
    tree.open.pop_back();
  }
}
void Builder::ResetMode() {
  for (size_t i = tree.open.size(); i-- > 0;) {
    const bool last = i == 0;
    DomString name(Name(tree.open[i]));
    if (last && !tree.context.empty()) name = tree.context;  // a fragment's context element
    if (Cell(name) && !last) { tree.mode = Mode::InCell; return; }
    if (name == u"tr") { tree.mode = Mode::InRow; return; }
    if (Section(name)) { tree.mode = Mode::InTableBody; return; }
    if (name == u"caption") { tree.mode = Mode::InCaption; return; }
    if (name == u"colgroup") { tree.mode = Mode::InColumnGroup; return; }
    if (name == u"table") { tree.mode = Mode::InTable; return; }
    if (name == u"body" || name == u"template" || name == u"frameset") { tree.mode = Mode::InBody; return; }
    if (name == u"html" && tree.context.empty()) { tree.mode = tree.head ? Mode::AfterHead : Mode::BeforeHead; return; }
    if (last) { tree.mode = Mode::InBody; return; }
  }
  tree.mode = Mode::InBody;
}
bool Builder::Table(Token& token) {
  switch (tree.mode) {
    case Mode::InTable: return InTable(*this, token);
    case Mode::InTableText: return InTableText(*this, token);
    case Mode::InCaption: return InCaption(*this, token);
    case Mode::InColumnGroup: return InColumnGroup(*this, token);
    case Mode::InTableBody: return InTableBody(*this, token);
    case Mode::InRow: return InRow(*this, token);
    case Mode::InCell: return InCell(*this, token);
    default: return false;
  }
}
// "in body" start tags of the table and select families: true when handled.
bool Builder::BodyTableTag(Token& token) {
  const DomStringView name = token.name;
  // "in template": table parts directly in template contents open their table mode.
  if (Name(Current()) == u"template" && In(name, {u"caption", u"col", u"colgroup", u"tbody", u"td", u"tfoot", u"th", u"thead", u"tr"})) {
    tree.mode = In(name, {u"caption", u"colgroup", u"tbody", u"tfoot", u"thead"}) ? Mode::InTable
              : name == u"col" ? Mode::InColumnGroup : name == u"tr" ? Mode::InTableBody : Mode::InRow;
    while (tree.mode > Mode::AfterBody && !Table(token)) {}
    if (tree.mode <= Mode::AfterBody) InBody(token);
    return true;
  }
  if (In(name, {u"caption", u"col", u"colgroup", u"frame", u"tbody", u"td", u"tfoot", u"th", u"thead", u"tr"}))
    return true;  // parse error: ignored outside a table
  if (name == u"frameset") return true;  // frameset-ok is false once the body has content
  if (name == u"table") { CloseP(); Insert(token, true); tree.mode = Mode::InTable; return true; }
  const bool select = InScope(u"select", false);
  if (name == u"option" || name == u"optgroup") {
    if (select) ImpliedEnds(*this, name == u"option" ? DomStringView(u"optgroup") : DomStringView());
    else if (Name(Current()) == u"option") Pop(*this);
    Insert(token, true);
    return true;
  }
  if (name == u"select" && select) { PopUntil(u"select"); return true; }
  if ((name == u"input" || name == u"textarea") && select) PopUntil(u"select");
  if (name == u"hr") {
    CloseP();
    if (select) ImpliedEnds(*this);
    Insert(token, true);
    return true;
  }
  return false;
}
}  // namespace ruyijs::document_model::html::detail
