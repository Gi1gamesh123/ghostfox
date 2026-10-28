// The CSSStyleDeclaration port. Document Model installs the one
// CSSStyleDeclaration and CSSStyleProperties prototype; a declaration block
// another owner stores (a CSS rule's style, a computed style) answers those
// prototype members through this port. The runtime assembly binds it after the
// Styling owner installs and clears it before teardown.
#ifndef RUYIJS_DOCUMENT_MODEL_STYLE_PORT_H
#define RUYIJS_DOCUMENT_MODEL_STYLE_PORT_H
#include "js/CallArgs.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs::document_model {
enum class StyleMember : int {
  CssTextGet, CssTextSet, Length, ParentRule, Item, Value, Priority, Store, Remove,
  NamedGet, NamedSet
};
struct StyleDeclarationPort {
  // Whether the unwrapped |object| is a declaration block of the port's owner.
  bool (*owns)(JSObject* object) = nullptr;
  // Runs |member| on |self| with the caller's arguments and return slot. For
  // NamedGet/NamedSet, |property| is the canonical property name and
  // |accessor| the IDL attribute declaring it (an alias reports that one, as
  // Firefox's errors do); both are null otherwise.
  bool (*call)(void* context, JSContext*, StyleMember member, JS::HandleObject self, const JS::CallArgs& args,
               const char* property, const char* accessor) = nullptr;
  // Synchronous caller-realm owner; cleared with the original port.
  void* context = nullptr;
};
}  // namespace ruyijs::document_model
#endif
