// HTMLCanvasElement width/height content-attribute notice (Firefox
// HTMLCanvasElement::AfterSetAttr). Every DOM path that sets or removes the
// attribute reaches the one attribute sink; this owner reads the current
// values from its own tree and hands them to the Canvas owner, which resets
// the element's bitmap. No page method runs inside the mutation.
#include "document_model/state.h"

#include <cstring>

namespace ruyijs::document_model {
void CanvasDimensionChanged(State& state, NodeKey key, DomStringView name) {
  if (name != DomStringView(u"width") && name != DomStringView(u"height")) return;
  if (!state.canvasDimensions) return;
  NodeSnapshot node;
  if (!state.Check(state.model.Inspect(key, &node)) ||
      std::strcmp(node.interface_name, "HTMLCanvasElement")) return;
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.page);
  JS::RootedValue element(cx), width(cx), height(cx);
  std::optional<DomString> w, h;
  if (!state.Check(state.model.GetAttribute(key, u"width", &w)) ||
      !state.Check(state.model.GetAttribute(key, u"height", &h)) ||
      !Value(cx, w, &width) || !Value(cx, h, &height) || !state.Wrap(key, &element)) return;
  JS::RootedObject canvas(cx, &element.toObject());
  state.canvasDimensions(state.canvasDimensionsContext, canvas, width, height);
}
}  // namespace ruyijs::document_model
