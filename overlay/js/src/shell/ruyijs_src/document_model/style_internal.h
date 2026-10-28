// Shared declaration-block helpers for inline style and data-* reflection.
// Owner: document_model (T4). Pure text handling only; the element's style
// attribute stays the single store.
#ifndef RUYIJS_DOCUMENT_MODEL_STYLE_INTERNAL_H
#define RUYIJS_DOCUMENT_MODEL_STYLE_INTERNAL_H
#include "document_model/state.h"
#include "js/Object.h"
#include "js/StableStringChars.h"
#include <vector>

namespace ruyijs::document_model {
inline bool Blank(char16_t c) {
  return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r' || c == u'\f';
}
inline DomStringView Trim(DomStringView text) {
  size_t begin = 0, end = text.size();
  while (begin < end && Blank(text[begin])) ++begin;
  while (end > begin && Blank(text[end - 1])) --end;
  return text.substr(begin, end - begin);
}
inline char16_t Fold(char16_t c) { return c >= u'A' && c <= u'Z' ? char16_t(c + 32) : c; }
inline DomString Lower(DomStringView text) {
  DomString out;
  for (char16_t c : text) out.push_back(Fold(c));
  return out;
}
// CSSOM serializes a camelCase IDL attribute back to its dashed property name.
inline DomString Dashed(DomStringView name) {
  DomString out;
  for (char16_t c : name) {
    if (c >= u'A' && c <= u'Z') { out.push_back(u'-'); c = char16_t(c + 32); }
    out.push_back(c);
  }
  return out;
}
// Inline style block operations through the Styling owner (style_block.cpp);
// |block| is the style attribute text, the returned block its new value.
size_t InlineCount(DomStringView block);
DomString InlineItem(DomStringView block, size_t index);
DomString InlineValue(DomStringView block, DomStringView name, bool* important);
DomString InlineText(DomStringView block);
DomString InlineSet(DomStringView block, DomStringView name, DomStringView value, bool important,
                    bool remove, DomString* old);
// The creation State only owns the accessor factory, never the element data.
inline State* StyleOwner(JSObject* self) {
  return static_cast<State*>(JS::GetReservedSlot(self, 1).toPrivate());
}
inline JSObject* StyleElement(JSObject* self) {
  return &JS::GetReservedSlot(self, 0).toObject();
}
inline bool CopyText(JSContext* cx, JS::HandleString text, DomString* result) {
  JS::AutoStableStringChars chars(cx);
  if (!chars.initTwoByte(cx, text)) return false;
  result->assign(chars.twoByteChars(), chars.length());
  return true;
}
bool ReflectedReceiver(JSContext*, JS::HandleValue, bool dataset, JS::MutableHandleObject);
// The receiver of a CSSStyleDeclaration/CSSStyleProperties member: an inline
// style facade (*foreign false) or a declaration block the style port owns.
// Anything else throws Firefox's "'<label>' called on an object that does not
// implement interface <iface>." TypeError. Read before touching args.rval().
bool StyleReceiver(JSContext*, const JS::CallArgs&, const char* label, const char* iface,
                   JS::MutableHandleObject self, bool* foreign);
// Runs |member| on a port-owned declaration block with the caller's arguments.
bool ForwardStyle(JSContext*, State&, const JS::CallArgs&, StyleMember, JS::HandleObject self,
                  const char* property = nullptr, const char* accessor = nullptr);
Wrapper* ReflectedElement(JSContext*, JS::HandleObject);
bool ReadStyle(JSContext*, JS::HandleObject element, DomString* out);
bool IsStyleObject(JSObject* object);
bool PrepareInlineValue(DomStringView name, DomString* value);
bool WriteStyle(JSContext*, JS::HandleObject element, DomStringView text);
}  // namespace ruyijs::document_model
#endif
