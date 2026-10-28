// Content Security owner ports of the Document Model (docs/owners/
// CONTENT_SECURITY.md). This owner only observes its unique tree: a
// <meta http-equiv="Content-Security-Policy"> that connects inside the main
// document's <head> hands its content to web_security (donor
// HTMLMetaElement::BindToTree), and an event handler content attribute asks
// web_security before it compiles. No policy state lives here.
#include "document_model/state.h"

#include <cstring>

namespace ruyijs::document_model {
namespace {
bool AsciiEqualsIgnoreCase(DomStringView value, const char* ascii) {
  size_t i = 0;
  for (; ascii[i]; ++i) {
    if (i >= value.size()) return false;
    char16_t c = value[i];
    if (c >= u'A' && c <= u'Z') c = char16_t(c - u'A' + u'a');
    if (c != char16_t(ascii[i])) return false;
  }
  return i == value.size();
}
}  // namespace

// Called from the structural-change notice for the main document, after the
// parser or a DOM edit connected `inserted`, in tree order.
void SecurityMetaInserted(State& state, const std::vector<NodeKey>& inserted) {
  if (!state.cspMeta) return;
  DocumentSnapshot document;
  if (state.model.InspectDocument(state.main, &document).code != ErrorCode::None) return;
  if (!document.head.document && !document.head.node) return;
  for (NodeKey scope : inserted) {
    if (scope.document != state.main.document) continue;
    std::vector<NodeKey> nodes;
    if (state.model.Subtree(scope, &nodes).code != ErrorCode::None) continue;
    for (NodeKey key : nodes) {
      NodeSnapshot node;
      if (state.model.Inspect(key, &node).code != ErrorCode::None || !node.is_connected ||
          !node.is_html_element || std::strcmp(node.interface_name, "HTMLMetaElement")) continue;
      std::optional<DomString> equiv, content;
      if (state.model.GetAttribute(key, u"http-equiv", &equiv).code != ErrorCode::None || !equiv ||
          !AsciiEqualsIgnoreCase(*equiv, "content-security-policy")) continue;
      bool in_head = false;
      if (state.model.AncestorOrSelf(document.head, key, &in_head).code != ErrorCode::None ||
          !in_head) continue;
      if (state.model.GetAttribute(key, u"content", &content).code != ErrorCode::None || !content)
        continue;
      state.cspMeta(state.cspContext, *content);
    }
  }
}

// Runs when an event handler content attribute is set or its element is
// inserted, as the donor's EventListenerManager::SetEventHandler checks CSP.
bool SecurityAllowsHandler(State& state, JS::HandleObject wrapper, JS::HandleString source,
                           bool* allowed) {
  *allowed = true;
  if (!state.cspHandler) return true;
  return state.cspHandler(state.cspContext, wrapper, source, allowed);
}

void Realm::SetSecurityPorts(CspMetaPort meta, CspInlinePort handler, void* context) {
  state_->cspMeta = meta;
  state_->cspHandler = handler;
  state_->cspContext = context;
}

bool Realm::NodeConnected(JS::HandleObject node) {
  Wrapper* entry = state_->Find(node.get());
  if (!entry || entry->dead || entry->key.document != state_->main.document) return false;
  NodeSnapshot snapshot;
  return state_->model.Inspect(entry->key, &snapshot).code == ErrorCode::None && snapshot.is_connected;
}
}  // namespace ruyijs::document_model
