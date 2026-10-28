// HTML tree construction (docs/DEVIATIONS.md D7): html/head/body modes, implied
// p/li/dd/dt/heading ends, raw text, generic end tags, template contents in head,
// foreign content, the table modes with foster parenting (model_tree_table.cpp) and
// the relaxed select rules. No adoption agency.
#include "document_model/model_tree_builder.h"

namespace ruyijs::document_model::html {
using namespace detail;
static NodeId Step(domain::DocumentAggregate& document, TreeState& tree, Token& token, const Budget& budget) {
  Builder builder{document, tree, budget};
  if (token.kind != Kind::Text) tree.skip_newline = false;
  if (tree.raw) {  // The "text" insertion mode: raw content, then its end tag.
    if (token.kind == Kind::Text) { builder.ScriptSource(token); builder.Text(token.data); return 0; }
    NodeId element = builder.Current();
    tree.open.pop_back();
    tree.raw = false;
    // A script cut off by end of input, or inside template contents, is never executed.
    if (token.kind == Kind::EndTag)
      return tree.scripts && token.name == u"script" && tree.templates.empty() ? element : 0;
  }
  // End of input while table characters are pending flushes them first.
  if (token.kind == Kind::End && tree.mode == Mode::InTableText) builder.Table(token);
  if (token.kind == Kind::End && tree.mode >= Mode::InBody) return 0;
  if (builder.Foreign(token)) return 0;
  for (bool again = true; again;) {
    if (tree.mode > Mode::AfterBody) {  // A table mode; false: reprocess in the mode it left for.
      if (builder.Table(token)) return 0;
      continue;
    }
    if (tree.mode == Mode::AfterBody) {
      if (token.kind == Kind::Comment) { builder.Comment(token, tree.html ? tree.html : tree.root); return 0; }
      if (token.kind == Kind::Doctype || token.kind == Kind::End ||
          (token.kind == Kind::Text && !builder.Leading(token, true)) ||
          (token.kind == Kind::EndTag && (token.name == u"html" || token.name == u"body"))) return 0;
      tree.mode = Mode::InBody;
    }
    if (tree.mode == Mode::InBody) { builder.InBody(token); return 0; }
    again = builder.BeforeBody(token);
  }
  return 0;
}
NodeId Build(domain::DocumentAggregate& document, TreeState& tree, Token& token, const Budget& budget) {
  const NodeId script = Step(document, tree, token, budget);
  tree.foreign = Builder{document, tree, budget}.ForeignInput();
  // Templates that left the open stack, innermost first, hand over their children.
  const bool end = token.kind == Kind::End;
  for (size_t i = tree.templates.size(); i-- > 0;) {
    const NodeId id = tree.templates[i];
    if (!end && std::find(tree.open.begin(), tree.open.end(), id) != tree.open.end()) continue;
    tree.templates.erase(tree.templates.begin() + i);
    if (tree.template_sink && tree.template_sink->closed)
      tree.template_sink->closed(tree.template_sink->context, tree.template_sink->document, id);
  }
  return script;
}
}
