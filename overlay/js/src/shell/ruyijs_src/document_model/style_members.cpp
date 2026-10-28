// CSSStyleDeclaration prototype members (T4), installed once for every
// declaration kind. Owner: document_model; an inline style's attribute stays
// its single store, and rule/computed declarations answer through the style
// declaration port (style_port.h).
#include "document_model/style_internal.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"

#include <iterator>
#include <string>

namespace ruyijs::document_model {
namespace {
struct MemberSpec {
  const char* name;
  StyleMember member;
  unsigned length;
};
constexpr MemberSpec kMethods[] = {
  {"item", StyleMember::Item, 1}, {"getPropertyValue", StyleMember::Value, 1},
  {"getPropertyPriority", StyleMember::Priority, 1}, {"setProperty", StyleMember::Store, 2},
  {"removeProperty", StyleMember::Remove, 1}
};

bool Load(JSContext* cx, JS::HandleObject self, DomString* block) {
  JS::RootedObject element(cx, StyleElement(self));
  return ReadStyle(cx, element, block);
}
bool Save(JSContext* cx, JS::HandleObject self, DomStringView block) {
  JS::RootedObject element(cx, StyleElement(self));
  return WriteStyle(cx, element, block);
}
// The receiver of an attribute: an inline facade in |self| (true, *done false),
// or a port-owned block whose member already ran (*done true).
bool Attribute(JSContext* cx, const JS::CallArgs& args, const char* label, StyleMember member,
               JS::MutableHandleObject self, bool* done) {
  State& state = *FunctionState(args);
  bool foreign = false;
  *done = false;
  if (!StyleReceiver(cx, args, label, "CSSStyleDeclaration", self, &foreign)) return false;
  if (!foreign) return true;
  *done = true;
  return ForwardStyle(cx, state, args, member, self);
}
bool Name(JSContext* cx, JS::HandleValue value, DomString* out) {
  DomString text;
  if (!String(cx, value, &text)) return false;
  *out = Lower(text);
  return true;
}
bool CssTextGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  bool done = false;
  if (!Attribute(cx, args, "get cssText", StyleMember::CssTextGet, &self, &done)) return false;
  if (done) return true;
  DomString block;
  if (!Load(cx, self, &block)) return false;
  return Value(cx, DomStringView(InlineText(block)), args.rval());
}
bool CssTextSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  bool done = false;
  if (!Attribute(cx, args, "set cssText", StyleMember::CssTextSet, &self, &done)) return false;
  if (done) return true;
  DomString text;
  if (!String(cx, args.get(0), &text, true)) return false;
  if (!Save(cx, self, InlineText(text))) return false;
  args.rval().setUndefined(); return true;
}
bool LengthGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  bool done = false;
  if (!Attribute(cx, args, "get length", StyleMember::Length, &self, &done)) return false;
  if (done) return true;
  DomString block;
  if (!Load(cx, self, &block)) return false;
  args.rval().setNumber(double(InlineCount(block)));
  return true;
}
bool ParentRuleGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  bool done = false;
  if (!Attribute(cx, args, "get parentRule", StyleMember::ParentRule, &self, &done)) return false;
  if (done) return true;
  DomString block;
  if (!Load(cx, self, &block)) return false;
  args.rval().setNull();
  return true;
}
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  const MemberSpec& spec = kMethods[FunctionSlot(args)];
  JS::RootedObject self(cx);
  bool foreign = false;
  if (!StyleReceiver(cx, args, spec.name, "CSSStyleDeclaration", &self, &foreign)) return false;
  if (argc < spec.length) {
    std::string message = std::string("CSSStyleDeclaration.") + spec.name + ": At least " +
        std::to_string(spec.length) + (spec.length == 1 ? " argument" : " arguments") +
        " required, but only " + std::to_string(argc) + " passed";
    return TypeError(cx, message.c_str());
  }
  if (foreign) return ForwardStyle(cx, state, args, spec.member, self);
  StyleMember method = spec.member;
  DomString block;
  if (method == StyleMember::Item) {
    uint32_t index = 0;
    if (!JS::ToUint32(cx, args[0], &index) || !Load(cx, self, &block)) return false;
    return Value(cx, DomStringView(InlineItem(block, index)), args.rval());
  }
  if (method == StyleMember::Value || method == StyleMember::Priority || method == StyleMember::Remove) {
    DomString name;
    if (!Name(cx, args[0], &name) || !Load(cx, self, &block)) return false;
    if (method == StyleMember::Remove) {
      DomString old;
      const DomString next = InlineSet(block, name, DomStringView(), false, true, &old);
      // Nothing removed leaves the attribute untouched (no mutation).
      if (InlineCount(next) != InlineCount(block) && !Save(cx, self, next)) return false;
      return Value(cx, DomStringView(old), args.rval());
    }
    bool important = false;
    const DomString value = InlineValue(block, name, &important);
    if (method == StyleMember::Value) return Value(cx, DomStringView(value), args.rval());
    return Value(cx, DomStringView(!value.empty() && important ? u"important" : u""), args.rval());
  }
  DomString name, value;
  if (!Name(cx, args.get(0), &name) || !String(cx, args.get(1), &value, true)) return false;
  DomString priority;
  if (!args.get(2).isUndefined() && !String(cx, args[2], &priority)) return false;
  args.rval().setUndefined();
  priority = Lower(priority);
  if (!value.empty() && ((!priority.empty() && priority != u"important") ||
                         !PrepareInlineValue(name, &value))) return true;
  // Conversions may mutate or adopt the element; read the current block now.
  if (!Load(cx, self, &block)) return false;
  const bool important = priority == u"important";
  if (!Save(cx, self, InlineSet(block, name, value, important, value.empty(), nullptr))) return false;
  args.rval().setUndefined();
  return true;
}
}  // namespace

bool InstallStyleDeclaration(State& state) {
  if (!state.Accessor("CSSStyleDeclaration", "cssText", CssTextGet, 0, CssTextSet)) return false;
  if (!state.Accessor("CSSStyleDeclaration", "length", LengthGet, 0)) return false;
  if (!state.Accessor("CSSStyleDeclaration", "parentRule", ParentRuleGet, 0)) return false;
  for (size_t i = 0; i < std::size(kMethods); ++i)
    if (!state.Method("CSSStyleDeclaration", kMethods[i].name, Call, kMethods[i].length, int32_t(i)))
      return false;
  return true;
}
}  // namespace ruyijs::document_model
