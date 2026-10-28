// document.open()/close(): the HTML "document open steps" start a
// script-created parser on the Document; document.write feeds it through
// ParserWrite and close() inserts the explicit end of its input.
#include "document_model/model_internal.h"

namespace ruyijs::document_model {
Status Model::OpenParse(NodeKey key, bool* opened) noexcept {
  return Protect([&] {
    *opened = false;
    auto& owner = State().document(key);
    if (!owner.uses_html_syntax())
      throw domain::DomError(domain::DomErrorCode::InvalidState, "document.open is only supported on HTML documents");
    // A script that the document's own parser is running (insertion point
    // defined): the open steps return without changing the document.
    auto found = State().parsers.find(key.document);
    if (found != State().parsers.end() && found->second &&
        found->second->insertion != html::Session::kNoInsertion) return;
    auto session = std::make_unique<html::Session>(DomString());
    session->tree.root = owner.id();
    session->insertion = 0;
    session->script_created = true;
    BindParserNotice(*session, State(), key.document);
    State().parsers[key.document] = std::move(session);  // Replaces an unfinished load.
    try {
      owner.set_parsed_doctype({});
      const auto children = owner.require(owner.id()).child_ids();
      for (auto child : children) owner.remove_child(owner.id(), child);  // Detach, never destroy.
      State().NoteRemoved(key, children, {}, {});
    } catch (...) {
      State().parsers.erase(key.document);
      throw;
    }
    owner.set_ready_state(domain::ReadyState::Loading);
    owner.set_current_script(0);
    State().Touch();
    *opened = true;
  });
}
Status Model::CloseParse(NodeKey key) noexcept {
  return Protect([&] {
    auto& owner = State().document(key);
    auto found = State().parsers.find(key.document);
    // close() ends only a script-created parser (no effect on a load).
    if (found == State().parsers.end() || !found->second || !found->second->script_created) return;
    auto& session = *found->second;
    const html::Budget budget = [&](size_t nodes, size_t units) { State().budget(nodes, units); };
    session.tokenizer.Rebase(session.input);
    try {
      for (;;) {
        html::Token token = session.tokenizer.Next();
        const bool end = token.kind == html::Token::Kind::End;
        html::Build(owner, session.tree, token, budget);
        if (end) break;
      }
    } catch (...) {
      State().parsers.erase(found);
      throw;
    }
    State().parsers.erase(found);
    owner.set_ready_state(domain::ReadyState::Complete);
    State().Touch();
  });
}
}  // namespace ruyijs::document_model
