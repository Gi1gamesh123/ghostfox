// XSLTProcessor (Firefox 158 XSLTProcessor.webidl, Exposed=Window): the processor
// object and its parameter table. Parameters are keyed by (namespaceURI, localName)
// and keep the converted XSLTParameterValue (number, boolean, string or node), as
// txMozillaXSLTProcessor does. This runtime has no XSLT engine (D151): with no
// stylesheet imported, a transform fails as Firefox's does (NS_ERROR_NOT_INITIALIZED);
// an imported stylesheet is kept but a transform still reports that failure.
#include "document_model/state.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include "js/MapAndSet.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
#include <iterator>
#include <string>

namespace ruyijs::document_model {
const JSClass kXsltClass = {"XSLTProcessor", JSCLASS_HAS_RESERVED_SLOTS(2)};  // 0 parameters Map, 1 stylesheet
namespace {
enum Op { kImport, kToFragment, kToDocument, kSet, kGet, kRemove, kClear, kReset };
const char* const kNames[] = {"importStylesheet", "transformToFragment", "transformToDocument", "setParameter",
                              "getParameter", "removeParameter", "clearParameters", "reset"};
const unsigned kLengths[] = {1, 2, 1, 3, 2, 2, 0, 0};

bool Fail(JSContext* cx, const std::string& text) { return TypeError(cx, text.c_str()); }
// [LegacyNullToEmptyString] namespaceURI + localName -> one Map key.
bool Key(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleValue key) {
  JS::RootedString ns(cx, args[0].isNull() ? JS_GetEmptyString(cx) : JS::ToString(cx, args[0]));
  JS::RootedString local(cx, ns ? JS::ToString(cx, args[1]) : nullptr);
  JS::RootedString separator(cx, local ? JS_NewStringCopyZ(cx, "\x1f") : nullptr);
  JS::RootedString head(cx, separator ? JS_ConcatStrings(cx, ns, separator) : nullptr);
  JSString* joined = head ? JS_ConcatStrings(cx, head, local) : nullptr;
  if (!joined) return false;
  key.setString(joined);
  return true;
}
// XSLTParameterValue: (unrestricted double or boolean or DOMString or Node or
// sequence<Node> or XPathResult), converted in WebIDL union order.
bool ParameterValue(JSContext* cx, JS::HandleValue value, JS::MutableHandleValue out) {
  if (value.isNumber() || value.isBoolean()) { out.set(value); return true; }
  if (value.isObject()) {
    JS::RootedValue probe(cx, value);
    if (NodeReceiver(cx, probe)) { out.set(value); return true; }
    JS_ClearPendingException(cx);
    JS::ForOfIterator it(cx);
    if (!it.init(value, JS::ForOfIterator::AllowNonIterable)) return false;
    if (it.valueIsIterable()) {
      JS::RootedObject list(cx, JS::NewArrayObject(cx, 0));
      JS::RootedValue item(cx);
      uint32_t index = 0;
      for (bool done = false;;) {
        if (!list || !it.next(&item, &done)) return false;
        if (done) break;
        const std::string branch = "XSLTProcessor.setParameter: Element of sequence<Node> branch of "
            "(unrestricted double or boolean or DOMString or Node or sequence<Node> or XPathResult)";
        if (!item.isObject()) return Fail(cx, branch + " is not an object.");
        if (!NodeReceiver(cx, item)) { JS_ClearPendingException(cx); return Fail(cx, branch + " does not implement interface Node."); }
        if (!JS_DefineElement(cx, list, index++, item, JSPROP_ENUMERATE)) return false;
      }
      out.setObject(*list);
      return true;
    }
  }
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text) return false;
  out.setString(text);
  return true;
}
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  const int op = FunctionSlot(args);
  JSObject* raw = args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr;
  if (!raw || JS::GetClass(raw) != &kXsltClass)
    return Fail(cx, std::string("'") + kNames[op] + "' called on an object that does not implement interface XSLTProcessor.");
  JS::RootedObject self(cx, raw), parameters(cx, &JS::GetReservedSlot(raw, 0).toObject());
  state.Observe("XSLTProcessor", kNames[op], int(ApiAccess::Call));
  if (argc < kLengths[op])
    return Fail(cx, std::string("XSLTProcessor.") + kNames[op] + ": At least " + std::to_string(kLengths[op]) +
                    " argument" + (kLengths[op] == 1 ? "" : "s") + " required, but only " + std::to_string(argc) + " passed");
  args.rval().setUndefined();
  JS::RootedValue key(cx), value(cx);
  switch (op) {
    case kImport:
      if (!args[0].isObject()) return Fail(cx, "XSLTProcessor.importStylesheet: Argument 1 is not an object.");
      if (!NodeReceiver(cx, args[0])) {
        JS_ClearPendingException(cx);
        return Fail(cx, "XSLTProcessor.importStylesheet: Argument 1 does not implement interface Node.");
      }
      JS::SetReservedSlot(self, 1, args[0]);
      return true;
    case kToFragment: case kToDocument: {
      const std::string where = std::string("XSLTProcessor.") + kNames[op];
      if (!args[0].isObject()) return Fail(cx, where + ": Argument 1 is not an object.");
      if (!NodeReceiver(cx, args[0])) { JS_ClearPendingException(cx); return Fail(cx, where + ": Argument 1 does not implement interface Node."); }
      if (op == kToFragment) {
        if (!args[1].isObject()) return Fail(cx, where + ": Argument 2 is not an object.");
        Wrapper* output = NodeReceiver(cx, args[1]);
        NodeSnapshot node;
        if (!output || !output->owner->Check(output->owner->model.Inspect(output->key, &node)) ||
            node.kind != NodeKind::Document) {
          JS_ClearPendingException(cx);
          return Fail(cx, where + ": Argument 2 does not implement interface Document.");
        }
      }
      return ThrowDOM(state, "NS_ERROR_NOT_INITIALIZED", "");
    }
    case kSet:
      return Key(cx, args, &key) && ParameterValue(cx, args[2], &value) && JS::MapSet(cx, parameters, key, value);
    case kGet:
      if (!Key(cx, args, &key) || !JS::MapGet(cx, parameters, key, args.rval())) return false;
      if (args.rval().isUndefined()) args.rval().setNull();
      return true;
    case kRemove: {
      bool removed = false;
      return Key(cx, args, &key) && JS::MapDelete(cx, parameters, key, &removed);
    }
    case kReset:
      JS::SetReservedSlot(self, 1, JS::UndefinedValue());
      [[fallthrough]];
    default:
      return JS::MapClear(cx, parameters);
  }
}
}  // namespace

bool NewXsltProcessor(State& state, const JS::CallArgs& args) {
  JS::RootedObject map(state.cx, JS::NewMapObject(state.cx));
  JS::RootedObject self(state.cx, map ? JS_NewObjectForConstructor(state.cx, &kXsltClass, args) : nullptr);
  if (!self) return false;
  JS::SetReservedSlot(self, 0, JS::ObjectValue(*map));
  args.rval().setObject(*self);
  return true;
}

bool InstallXsltProcessor(State& state) {
  JS::RootedObject plain(state.cx, JS::GetRealmObjectPrototype(state.cx));
  if (!state.DefineInterface("XSLTProcessor", plain, NewXslt, false)) return false;  // constructor(): new only
  for (int op = 0; op < int(std::size(kNames)); ++op)
    if (!state.Method("XSLTProcessor", kNames[op], Call, kLengths[op], op)) return false;
  return true;
}
}  // namespace ruyijs::document_model
