// Sanitizer interface members (Firefox 158 Sanitizer.webidl): constructor, get()
// with Firefox's sorted output, and the allow/remove/replace/set operations over
// the instance's configuration (a sz::Config in reserved slot 0).
#include "document_model/sanitizer.h"
#include "runtime/runtime.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
#include <algorithm>
#include <iterator>
#include <new>

namespace ruyijs::document_model {
namespace {
void Finalize(JS::GCContext*, JSObject* object) {
  const JS::Value slot = JS::GetReservedSlot(object, 0);
  if (!slot.isUndefined()) delete static_cast<sz::Config*>(slot.toPrivate());
}
const JSClassOps kOps = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, Finalize};
const JSClass kSanitizerClass = {"Sanitizer", JSCLASS_HAS_RESERVED_SLOTS(1) | JSCLASS_FOREGROUND_FINALIZE, &kOps};
enum Op { kGet, kAllowElement, kRemoveElement, kReplace, kAllowAttribute, kRemoveAttribute, kComments, kData,
          kJavascript, kRemoveUnsafe };
const char* const kNames[] = {"get", "allowElement", "removeElement", "replaceElementWithChildren", "allowAttribute",
                              "removeAttribute", "setComments", "setDataAttributes", "setJavascriptURLs", "removeUnsafe"};
const unsigned kLengths[] = {0, 1, 1, 1, 1, 1, 1, 1, 1, 0};

bool Fail(JSContext* cx, const std::string& text) { return TypeError(cx, text.c_str()); }
bool String(JSContext* cx, const std::string& text, JS::MutableHandleValue out) {
  JSString* s = JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(text.data(), text.size()));
  if (!s) return false;
  out.setString(s);
  return true;
}
bool NameObject(JSContext* cx, const sz::Name& n, JS::MutableHandleObject out) {
  out.set(JS_NewPlainObject(cx));
  JS::RootedValue v(cx);
  if (!out || !String(cx, n.name, &v) || !JS_DefineProperty(cx, out, "name", v, JSPROP_ENUMERATE)) return false;
  if (n.nullNs) v.setNull(); else if (!String(cx, n.ns, &v)) return false;
  return JS_DefineProperty(cx, out, "namespace", v, JSPROP_ENUMERATE);
}
bool NameList(JSContext* cx, sz::Set names, JS::MutableHandleValue out) {
  std::sort(names.begin(), names.end());
  JS::RootedObject list(cx, JS::NewArrayObject(cx, 0)), item(cx);
  if (!list) return false;
  for (uint32_t i = 0; i < names.size(); ++i)
    if (!NameObject(cx, names[i], &item) || !JS_DefineElement(cx, list, i, item, JSPROP_ENUMERATE)) return false;
  out.setObject(*list);
  return true;
}
bool Define(JSContext* cx, JS::HandleObject target, const char* key, JS::HandleValue value) {
  return JS_DefineProperty(cx, target, key, value, JSPROP_ENUMERATE);
}
// https://wicg.github.io/sanitizer-api/#dom-sanitizer-get: keys in dictionary order.
bool Get(JSContext* cx, const sz::Config& c, JS::MutableHandleValue result) {
  JS::RootedObject out(cx, JS_NewPlainObject(cx));
  JS::RootedValue v(cx);
  if (!out) return false;
  if (c.attributes ? !NameList(cx, *c.attributes, &v) || !Define(cx, out, "attributes", v) : false) return false;
  v.setBoolean(c.comments);
  if (!Define(cx, out, "comments", v)) return false;
  if (c.dataAttributes) { v.setBoolean(*c.dataAttributes); if (!Define(cx, out, "dataAttributes", v)) return false; }
  if (c.elements) {
    auto elements = *c.elements;
    std::sort(elements.begin(), elements.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    JS::RootedObject list(cx, JS::NewArrayObject(cx, 0)), item(cx);
    if (!list) return false;
    for (uint32_t i = 0; i < elements.size(); ++i) {
      if (!NameObject(cx, elements[i].first, &item)) return false;
      const auto& a = elements[i].second;
      if (a.attributes && (!NameList(cx, *a.attributes, &v) || !Define(cx, item, "attributes", v))) return false;
      if (a.removeAttributes && (!NameList(cx, *a.removeAttributes, &v) || !Define(cx, item, "removeAttributes", v)))
        return false;
      if (!JS_DefineElement(cx, list, i, item, JSPROP_ENUMERATE)) return false;
    }
    v.setObject(*list);
    if (!Define(cx, out, "elements", v)) return false;
  }
  v.setBoolean(c.javascriptURLs);
  if (!Define(cx, out, "javascriptURLs", v)) return false;
  // processingInstructions/removeProcessingInstructions are behind a pref that is
  // off in Firefox 158, so get() never reports them.
  if (c.removeAttributes && (!NameList(cx, *c.removeAttributes, &v) || !Define(cx, out, "removeAttributes", v)))
    return false;
  if (c.removeElements && (!NameList(cx, *c.removeElements, &v) || !Define(cx, out, "removeElements", v))) return false;
  if (c.replaceWithChildrenElements &&
      (!NameList(cx, *c.replaceWithChildrenElements, &v) || !Define(cx, out, "replaceWithChildrenElements", v)))
    return false;
  result.setObject(*out);
  return true;
}
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  const int op = FunctionSlot(args);
  JSObject* raw = args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr;
  if (!raw || JS::GetClass(raw) != &kSanitizerClass)
    return Fail(cx, std::string("'") + kNames[op] + "' called on an object that does not implement interface Sanitizer.");
  sz::Config& c = *static_cast<sz::Config*>(JS::GetReservedSlot(raw, 0).toPrivate());
  state.Observe("Sanitizer", kNames[op], int(ApiAccess::Call));
  const std::string where = std::string("Sanitizer.") + kNames[op] + ": ";
  if (argc < kLengths[op]) return Fail(cx, where + "At least 1 argument required, but only 0 passed");
  if (op == kGet) return Get(cx, c, args.rval());
  bool result = false;
  std::string error;
  if (op == kComments || op == kData || op == kJavascript) {
    const bool allow = JS::ToBoolean(args[0]);
    result = op == kComments ? c.SetComments(allow) : op == kData ? c.SetDataAttributes(allow) : c.SetJavascriptURLs(allow);
  } else if (op == kRemoveUnsafe) {
    result = c.RemoveUnsafe(SanitizerEventAttributes());
  } else if (op == kAllowElement) {
    sz::Input::Element element;
    if (!SanitizerElement(cx, args[0], where, &element, &error)) return false;
    if (!error.empty()) return Fail(cx, error);
    result = c.AllowElement(element);
  } else {
    const bool element = op == kRemoveElement || op == kReplace;
    sz::Name name;
    if (!SanitizerName(cx, args[0], element ? sz::kHtml : nullptr, element ? sz::kHtml : nullptr, where, &name, &error))
      return false;
    if (!error.empty()) return Fail(cx, error);
    result = op == kRemoveElement ? c.RemoveElement(name) : op == kReplace ? c.ReplaceElementWithChildren(name)
           : op == kAllowAttribute ? c.AllowAttribute(name) : c.RemoveAttribute(name);
  }
  args.rval().setBoolean(result);
  return true;
}
}  // namespace

// new Sanitizer(optional (SanitizerConfig or SanitizerPresets) configuration = "default").
bool NewSanitizer(State& state, const JS::CallArgs& args) {
  JSContext* cx = state.cx;
  auto config = std::make_unique<sz::Config>();
  JS::RootedValue value(cx, args.get(0));
  if (value.isUndefined()) {
    SanitizerDefault(config.get());
  } else if (value.isNull() || value.isObject()) {
    sz::Input input;
    std::string error;
    JS::RootedObject dict(cx, value.isObject() ? &value.toObject() : JS_NewPlainObject(cx));
    if (!dict || !SanitizerInput(cx, dict, &input, &error)) return false;
    if (error.empty()) error = config->Set(input, true);
    if (!error.empty()) return Fail(cx, (error.rfind("Sanitizer constructor: ", 0) == 0 ? "" : "Sanitizer constructor: ") + error);
  } else {
    JS::RootedString text(cx, JS::ToString(cx, value));
    std::string preset;
    if (!text || !ToUTF8(cx, text, &preset)) return false;
    if (preset != "default")
      return Fail(cx, "Sanitizer constructor: '" + preset + "' (value of SanitizerPresets branch of (SanitizerConfig "
                      "or SanitizerPresets)) is not a valid value for enumeration SanitizerPresets.");
    SanitizerDefault(config.get());
  }
  JS::RootedObject self(cx, JS_NewObjectForConstructor(cx, &kSanitizerClass, args));
  if (!self) return false;
  JS::SetReservedSlot(self, 0, JS::PrivateValue(config.release()));
  args.rval().setObject(*self);
  return true;
}

bool InstallSanitizer(State& state) {
  JS::RootedObject plain(state.cx, JS::GetRealmObjectPrototype(state.cx));
  if (!state.DefineInterface("Sanitizer", plain, NewSanitizerKind, false)) return false;
  for (int op = 0; op < int(std::size(kNames)); ++op)
    if (!state.Method("Sanitizer", kNames[op], Call, kLengths[op], op)) return false;
  return true;
}
}  // namespace ruyijs::document_model
