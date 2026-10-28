#include "runtime/adopted_ports.h"
#include "runtime/browsing_context.h"

namespace ruyijs {
void BindAdoptedStyles(BrowsingContext& owner) {
  owner.styling->SetAdoptedPorts([](void* context, JS::HandleValue value,
      JS::MutableHandleValue out, bool shadow) {
    return static_cast<document_model::Realm*>(context)->ResolveAdoptedStyles(value, out, shadow);
  }, [](void* context, JS::HandleObject scope, JS::MutableHandleObject out) {
    return static_cast<document_model::Realm*>(context)->ReadStyleScopeDocument(scope, out);
  }, [](void* context, JS::HandleObject element, JS::MutableHandleObject out, bool* shadow) {
    return static_cast<document_model::Realm*>(context)->ReadCascadeScope(element, out, shadow);
  }, owner.document.get());
  owner.document->SetAdoptedStylesListPort([](void* context, JS::HandleObject scope,
      JS::MutableHandleValue out) {
    return static_cast<web_styling::Realm*>(context)->NewAdoptedList(scope, out);
  }, owner.styling.get());
}
}
