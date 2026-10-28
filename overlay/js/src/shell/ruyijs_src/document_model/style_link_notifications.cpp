// Discover external sources at committed changes, including unwrapped parser nodes.
#include "document_model/state.h"
#include "document_model/style_source_values.h"
#include "domain/styling/external_style_value.h"
#include "js/Exception.h"
#include "js/Realm.h"
namespace ruyijs::document_model {
bool NotifyLinkStyle(State& state, NodeKey key) {
  if (!state.styleElement) return true;
  if (JS_IsExceptionPending(state.cx)) return false;
  NodeSnapshot node;
  const Status status=state.model.Inspect(key,&node);
  if (status.code!=ErrorCode::None) return state.Check(status);
  if (std::string(node.interface_name) != "HTMLLinkElement") return true;
  // A link whose rel has no "stylesheet" token has no sheet (Styling's own
  // rule, domain::styling::StyleRelation). Asking for its sheet would make
  // Styling read every style source of the document per inserted link, which
  // is quadratic for pages with hundreds of prefetch links.
  std::optional<DomString> rel;
  const Status read = state.model.GetAttribute(key, u"rel", &rel);
  if (read.code != ErrorCode::None) return state.Check(read);
  std::string relation;
  if (rel) for (char16_t unit : *rel) relation.push_back(unit < 0x80 ? char(unit) : '?');
  const bool stylesheet = domain::styling::StyleRelation(relation, "stylesheet");
  JSAutoRealm realm(state.cx, state.page);
  JS::RootedValue ignored(state.cx);
  return state.styleElement(state.styleContext, key.document, key.node,
                              stylesheet ? StyleElementAction::Sheet : StyleElementAction::Forget, false, &ignored);
}
}
