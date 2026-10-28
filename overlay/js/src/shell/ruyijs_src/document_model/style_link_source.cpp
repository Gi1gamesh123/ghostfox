// Link metadata from the original tree. Loading and CSS state belong to Styling.
#include "document_model/state.h"
#include "document_model/style_source_values.h"
#include "document_model/model_hyperlink.h"
#include "browser_environment/environment.h"
namespace ruyijs::document_model {
namespace {
bool Attribute(State& state, NodeKey key, const char16_t* name, std::string* out,
                 bool* present = nullptr) {
  std::optional<DomString> value;
  if (!state.Check(state.model.GetAttribute(key, name, &value))) return false;
  if (present) *present = value.has_value();
  *out = value ? StyleSourceText(*value) : std::string();
  return true;
}
bool URL(State& state, const DocumentSnapshot& document, std::string* out) {
  if (document.url) { *out = StyleSourceText(*document.url); return true; }
  JS::RootedValue value(state.cx);
  if (!state.environment.URL(&value)) return false;
  DomString text;
  if (!String(state.cx, value, &text)) return false;
  *out=StyleSourceText(text);return true;
}
}
bool StyleDocumentURL(State& state, NodeKey node, std::string* out) {
  NodeSnapshot snapshot;
  DocumentSnapshot document;
  if (!state.Check(state.model.Inspect(node, &snapshot))) return false;
  const NodeKey owner = snapshot.kind == NodeKind::Document ? node : snapshot.owner_document;
  return state.Check(state.model.InspectDocument(owner, &document)) && URL(state, document, out);
}
bool ReadLinkStyleSource(State& state, NodeKey key, NodeKey scope,
                          const NodeSnapshot& node, StyleSheetSource* source) {
  source->document = key.document;source->key = key.node;
  source->scopeDocument = scope.document;source->scopeNode = scope.node;
  source->mainDocument = Same(scope, state.main);
  source->external = true;source->presented = Same(node.composed_root, state.main);
  bool disabled;
  std::string raw;
  if (!Attribute(state, key, u"href", &raw) ||
      !Attribute(state, key, u"rel", &source->rel) ||
      !Attribute(state, key, u"title", &source->titleAttribute) ||
      !Attribute(state, key, u"media", &source->media) ||
      !Attribute(state, key, u"type", &source->typeAttribute) ||
      !Attribute(state, key, u"crossorigin", &source->crossOrigin, &source->hasCrossOrigin) ||
      !Attribute(state, key, u"disabled", &source->text, &disabled)) return false;
  source->disabled = disabled;source->text.clear();
  source->title = node.shadow_host.node ? std::string() : StyleSourceTitle(source->titleAttribute);
  NodeSnapshot root;
  if (!state.Check(state.model.Inspect(scope, &root))) return false;
  if (root.shadow_host.node) source->title.clear();
  DocumentSnapshot document;
  if (!state.Check(state.model.InspectDocument(node.owner_document, &document)) ||
      !URL(state, document, &source->documentURL)) return false;
  std::string base = source->documentURL;
  std::vector<NodeKey> bases;
  if (!state.Check(state.model.Query(document.key, QueryKind::TagName, u"base", &bases))) return false;
  for (NodeKey candidate : bases) {
    std::string href;bool present;
    if (!Attribute(state, candidate, u"href", &href, &present)) return false;
    if (!present) continue;
    if (!state.Check(ReadHyperlink(href, base, HyperlinkField::Href, &base))) return false;
    break;
  }
  if (raw.empty()) return true;
  return state.Check(ReadHyperlink(raw, base, HyperlinkField::Href, &source->href)) &&
      state.Check(ReadHyperlink(source->documentURL, "", HyperlinkField::Origin, &source->origin)) &&
      state.Check(ReadHyperlink(source->href, "", HyperlinkField::Origin, &source->resourceOrigin));
}
}
