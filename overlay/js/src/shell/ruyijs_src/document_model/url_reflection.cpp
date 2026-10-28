// Reflected URL content attributes (HTML "reflect a USVString as a URL";
// donor nsGenericHTMLElement::GetURIAttr): an absent attribute reads "", a value
// that resolves against the node document's URL reads the serialized absolute
// URL, and one that does not resolve reads the raw attribute value.
#include "document_model/state.h"
#include "document_model/model_hyperlink.h"
#include "browser_environment/environment.h"
#include "runtime/runtime.h"
#include "js/CharacterEncoding.h"

namespace ruyijs::document_model {
bool ReflectUrl(State& state, NodeKey key, const std::optional<DomString>& raw,
                JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  if (!raw) return Value(cx, DomStringView(), result);
  NodeSnapshot node;
  DocumentSnapshot document;
  if (!state.Check(state.model.Inspect(key, &node)) ||
      !state.Check(state.model.InspectDocument(node.owner_document, &document))) return false;
  JS::RootedValue base(cx), source(cx);
  if (document.url) {
    if (!Value(cx, DomStringView(*document.url), &base)) return false;
  } else if (!state.environment.URL(&base)) {
    return false;
  }
  if (!Value(cx, DomStringView(*raw), &source) || !base.isString()) return false;
  JS::RootedString b(cx, base.toString()), s(cx, source.toString());
  std::string baseText, sourceText, resolved;
  if (!ToUTF8(cx, b, &baseText) || !ToUTF8(cx, s, &sourceText)) return false;
  if (ReadHyperlink(sourceText, baseText, HyperlinkField::Href, &resolved).code != ErrorCode::None ||
      resolved.empty())
    return Value(cx, DomStringView(*raw), result);
  JSString* text = JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(resolved.data(), resolved.size()));
  if (!text) return false;
  result.setString(text);
  return true;
}
}  // namespace ruyijs::document_model
