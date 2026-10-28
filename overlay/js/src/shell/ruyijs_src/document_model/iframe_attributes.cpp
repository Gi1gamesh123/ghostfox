#include "document_model/iframe_attributes.h"
#include "document_model/state.h"
#include <cstring>
#include <optional>

namespace ruyijs::document_model {
namespace {
// Shared body of the two reflected content attributes. `url_attribute` selects
// the HTML rule: a URL attribute written as the empty string removes the
// attribute (`src`), while a DOMString attribute keeps the empty value
// (`srcdoc`). Both read and write the unique tree, so there is no second copy.
bool FrameGet(JSContext* cx, const JS::CallArgs& args, const char16_t* name,
              JS::MutableHandleValue result) {
  State& state = *FunctionState(args);
  NodeKind kind = NodeKind::Element;
  Wrapper* entry = Receiver(state, args, &kind);
  if (!entry) return false;
  NodeSnapshot node;
  if (!state.Check(state.model.Inspect(entry->key, &node))) return false;
  if (std::strcmp(node.interface_name, "HTMLIFrameElement"))
    return TypeError(cx, "HTMLIFrameElement member called on an incompatible receiver");
  std::optional<DomString> value;
  if (!state.Check(state.model.GetAttribute(entry->key, name, &value))) return false;
  // `src` reflects as a URL ("" when absent); `srcdoc` as its plain value.
  if (!std::char_traits<char16_t>::compare(name, u"src", 4)) return ReflectUrl(state, entry->key, value, result);
  return Value(cx, value ? DomStringView(*value) : DomStringView(), result);
}
bool FrameSet(JSContext* cx, const JS::CallArgs& args, const char16_t* name,
              bool url_attribute) {
  State& state = *FunctionState(args);
  NodeKind kind = NodeKind::Element;
  Wrapper* entry = Receiver(state, args, &kind);
  if (!entry) return false;
  NodeSnapshot node;
  if (!state.Check(state.model.Inspect(entry->key, &node))) return false;
  if (std::strcmp(node.interface_name, "HTMLIFrameElement"))
    return TypeError(cx, "HTMLIFrameElement member called on an incompatible receiver");
  DomString text;
  if (!String(cx, args.get(0), &text)) return false;
  if (url_attribute && text.empty())
    return state.Check(state.model.RemoveAttribute(entry->key, name));
  return state.Check(state.model.SetAttribute(entry->key, name, text));
}
bool SrcGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  return FrameGet(cx, args, u"src", args.rval());
}
bool SrcSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  return FrameSet(cx, args, u"src", true);
}
bool SrcdocGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  return FrameGet(cx, args, u"srcdoc", args.rval());
}
bool SrcdocSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  return FrameSet(cx, args, u"srcdoc", false);
}
}  // namespace
bool InstallIframeAttributes(State& state) {
  return state.Accessor("HTMLIFrameElement", "src", SrcGet, 0, SrcSet) &&
         state.Accessor("HTMLIFrameElement", "srcdoc", SrcdocGet, 0, SrcdocSet) &&
         InstallIframeMembers(state);
}
}  // namespace ruyijs::document_model
