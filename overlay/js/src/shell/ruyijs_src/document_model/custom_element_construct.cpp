// Custom element definitions and construction (HTML §4.13.4 define, §3.2.3
// "HTML element constructors", §4.13.3 "create an element" with the
// synchronous custom elements flag). docs/owners/DOCUMENT_MODEL.md 2026-10-21.
#include "document_model/state.h"
#include "runtime/exception_capture.h"
#include "js/Exception.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
CustomElementDefinition* FindDefinition(State& s, DomStringView name) {
  if (!s.customElements) return nullptr;
  for (auto& entry : s.customElements->definitions)
    if (DomStringView(entry->name) == name) return entry.get();
  return nullptr;
}
CustomElementDefinition* DefinitionFor(State& s, JSObject* constructor) {
  if (!s.customElements || !constructor) return nullptr;
  JSObject* target = js::CheckedUnwrapStatic(constructor);
  for (auto& entry : s.customElements->definitions)
    if (entry->constructor->get() == target) return entry.get();
  return nullptr;
}
namespace {
bool Strings(JSContext* cx, JS::HandleValue value, std::vector<std::u16string>* out) {
  if (value.isUndefined()) return true;
  JS::ForOfIterator it(cx);
  JS::RootedValue item(cx);
  if (!it.init(value)) return false;
  for (bool done = false;;) {
    if (!it.next(&item, &done)) return false;
    if (done) return true;
    DomString text;
    if (!String(cx, item, &text)) return false;
    out->push_back(std::move(text));
  }
}
}  // namespace

// define() step 14: prototype, then the lifecycle callbacks, then the static
// observedAttributes (only with attributeChangedCallback), disabledFeatures and
// formAssociated, in Firefox's order. Each read runs page code once, here.
bool ReadDefinition(State& s, CustomElementDefinition& d) {
  JSContext* cx = s.cx;
  JS::RootedObject ctor(cx, d.constructor->get());
  JS::RootedValue v(cx);
  if (!JS_GetProperty(cx, ctor, "prototype", &v)) return false;
  if (!v.isObject()) return TypeError(cx, "CustomElementRegistry.define: constructor.prototype is not an object");
  *d.prototype = &v.toObject();
  JS::RootedObject proto(cx, &v.toObject());
  struct Callback { const char* name; JS::PersistentRootedValue* slot; };
  const Callback callbacks[] = {{"connectedCallback", d.connected.get()},
                                {"disconnectedCallback", d.disconnected.get()},
                                {"adoptedCallback", d.adopted.get()},
                                {"attributeChangedCallback", d.attributeChanged.get()}};
  for (const auto& c : callbacks) {
    if (!JS_GetProperty(cx, proto, c.name, &v)) return false;
    if (!v.isUndefined() && (!v.isObject() || !JS::IsCallable(&v.toObject())))
      return TypeError(cx, (std::string("CustomElementRegistry.define: ") + c.name + " is not callable").c_str());
    c.slot->set(v);
  }
  if (d.attributeChanged->isObject()) {
    if (!JS_GetProperty(cx, ctor, "observedAttributes", &v) || !Strings(cx, v, &d.observed)) return false;
  }
  std::vector<std::u16string> disabled;
  if (!JS_GetProperty(cx, ctor, "disabledFeatures", &v) || !Strings(cx, v, &disabled)) return false;
  for (const auto& feature : disabled) {
    d.disableInternals |= feature == u"internals";
    d.disableShadow |= feature == u"shadow";
  }
  if (!JS_GetProperty(cx, ctor, "formAssociated", &v)) return false;
  d.formAssociated = JS::ToBoolean(v);
  return true;
}

// [HTMLConstructor]: during an upgrade the constructor returns the element on
// the construction stack; otherwise it creates a new element of the definition.
bool ConstructCustomElement(State& s, const JS::CallArgs& args) {
  JSContext* cx = s.cx;
  if (!args.isConstructing()) return TypeError(cx, "HTMLElement constructor: 'new' is required");
  JSObject* target = &args.newTarget().toObject();
  CustomElementDefinition* definition = DefinitionFor(s, target);
  if (!definition) return TypeError(cx, "Illegal constructor.");
  JS::RootedValue protoValue(cx);
  JS::RootedObject newTarget(cx, target);
  if (!JS_GetProperty(cx, newTarget, "prototype", &protoValue)) return false;
  auto& stack = s.customElements->constructing;
  if (!stack.empty() && stack.back().second == definition && stack.back().first.node) {
    const NodeKey key = stack.back().first;
    stack.back().first = NodeKey{};  // "already constructed" marker
    JS::RootedValue element(cx);
    if (!s.Wrap(key, &element)) return false;
    JS::RootedObject object(cx, &element.toObject());
    JS::RootedObject proto(cx, protoValue.isObject() ? &protoValue.toObject() : definition->prototype->get());
    if (!JS_SetPrototype(cx, object, proto)) return false;
    args.rval().set(element);
    return true;
  }
  if (!stack.empty() && stack.back().second == definition)
    return ThrowDOM(s, "InvalidStateError", "Custom element constructor was already called");
  CreateSpec spec{NodeKind::Element, DomStringView(definition->name), DomStringView()};
  Draft draft;
  if (!s.Check(s.model.Create(s.main, spec, &draft)) || !s.Commit(draft, args.rval(), &args)) return false;
  CustomElementState& state = s.customElements->states[draft.root];
  state.kind = CustomElementState::Custom;
  state.definition = definition;
  state.document = s.main;
  return true;
}

// createElement with a defined name: construct synchronously and check the
// result is a fresh element of that name (HTML "create an element" step 6).
bool CreateDefinedElement(State& s, CustomElementDefinition& definition, JS::MutableHandleValue result) {
  JSContext* cx = s.cx;
  JS::RootedValue ctor(cx, JS::ObjectValue(*definition.constructor->get()));
  JS::RootedObject created(cx);
  bool ok = JS::Construct(cx, ctor, JS::HandleValueArray::empty(), &created);
  if (ok) {
    JSObject* raw = js::CheckedUnwrapStatic(created);
    Wrapper* entry = raw && JS::GetClass(raw) == &kNodeClass ? s.Find(raw) : nullptr;
    NodeSnapshot node;
    if (!entry || !s.Check(s.model.Inspect(entry->key, &node)) || node.kind != NodeKind::Element ||
        !node.local_name || DomStringView(*node.local_name) != DomStringView(definition.name))
      ok = TypeError(cx, "Custom element constructor did not return a matching element");
    else if (node.first_child.node || node.parent.node)
      ok = ThrowDOM(s, "NotSupportedError", "Custom element constructor produced an element with children or a parent");
    if (ok) { result.setObject(*created); return true; }
  }
  // The exception is reported, and the result is an element whose custom
  // element state is "failed" (HTML "create an element" step 6.1.2).
  if (JS_IsThrowingOutOfMemory(cx)) return false;
  CaptureOperationFailure(cx);
  JS_ClearPendingException(cx);
  CreateSpec spec{NodeKind::Element, DomStringView(definition.name), DomStringView()};
  Draft draft;
  if (!s.Check(s.model.Create(s.main, spec, &draft)) || !s.Commit(draft, result)) return false;
  CustomElementState& state = s.customElements->states[draft.root];
  state.kind = CustomElementState::Failed;
  state.definition = &definition;
  state.document = s.main;
  return true;
}
}  // namespace ruyijs::document_model
