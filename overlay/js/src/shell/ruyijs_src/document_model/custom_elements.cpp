#include "document_model/state.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
#include "js/WeakMap.h"
#include <new>

namespace ruyijs::document_model {
namespace {
enum Operation { Define, Get, WhenDefined, Upgrade, GetName };

JSObject* Receiver(JSContext* cx, const JS::CallArgs& args, State& state) {
  JSObject* object = args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr;
  if (!state.customElements || object != state.customElements->object.get()) {
    TypeError(cx, "CustomElementRegistry receiver is incompatible");
    return nullptr;
  }
  return object;
}

bool Name(JSContext* cx, JS::HandleValue value, DomString* result) {
  if (!String(cx, value, result)) return false;
  if (result->size() < 3 || (*result)[0] < u'a' || (*result)[0] > u'z')
    return TypeError(cx, "Invalid custom element name");
  bool hyphen = false;
  for (char16_t c : *result) {
    const bool alpha = (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z');
    const bool digit = c >= u'0' && c <= u'9';
    if (!(alpha || digit || c == u'-' || c == u'.' || c == u'_'))
      return TypeError(cx, "Invalid custom element name");
    hyphen |= c == u'-';
  }
  if (!hyphen) return TypeError(cx, "Invalid custom element name");
  return true;
}

CustomElementDefinition* Find(State& state, DomStringView name) {
  for (auto& entry : state.customElements->definitions)
    if (entry->name == name) return entry.get();
  return nullptr;
}

bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  if (!Receiver(cx, args, state)) return false;
  const auto op = Operation(FunctionSlot(args));
  if (op == GetName) {
    if (!argc || !args[0].isObject() || !JS::IsCallable(&args[0].toObject()))
      return TypeError(cx, "CustomElementRegistry.getName requires a constructor");
    for (auto& entry : state.customElements->definitions)
      if (entry->constructor && entry->constructor->get() == &args[0].toObject()) {
        return Value(cx, DomStringView(entry->name), args.rval());
      }
    args.rval().setUndefined();
    return true;
  }
  if (op == Upgrade) {
    if (!argc || !args[0].isObject()) return TypeError(cx, "CustomElementRegistry.upgrade requires a Node");
    JSObject* node = js::CheckedUnwrapStatic(&args[0].toObject());
    Wrapper* entry = node && JS::GetClass(node) == &kNodeClass ? state.Find(node) : nullptr;
    if (!entry) return TypeError(cx, "CustomElementRegistry.upgrade requires a Node");
    args.rval().setUndefined();
    return UpgradeTree(state, entry->key, false);
  }
  if (!argc) return TypeError(cx, "CustomElementRegistry method requires a name");
  DomString name;
  if (!Name(cx, args[0], &name)) return false;
  if (op == Get || op == WhenDefined) {
    if (auto* entry = Find(state, name)) {
      if (op == Get) args.rval().setObject(*entry->constructor->get());
      else {
        JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
        JS::RootedValue ctor(cx, JS::ObjectValue(*entry->constructor->get()));
        if (!promise || !JS::ResolvePromise(cx, promise, ctor)) return false;
        args.rval().setObject(*promise);
      }
      return true;
    }
    if (op == Get) { args.rval().setUndefined(); return true; }
    JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
    if (!promise) return false;
    std::unique_ptr<CustomElementPending> pending(new (std::nothrow)
        CustomElementPending(cx, name, promise));
    if (!pending) { JS_ReportOutOfMemory(cx); return false; }
    state.customElements->pending.push_back(std::move(pending));
    args.rval().setObject(*promise);
    return true;
  }
  if (op != Define) return TypeError(cx, "Unsupported CustomElementRegistry operation");
  if (argc < 2 || !args[1].isObject() || !JS::IsCallable(&args[1].toObject()))
    return TypeError(cx, "Custom element constructor is not callable");
  if (Find(state, name)) return ThrowDOM(state, "NotSupportedError", "Duplicate custom element definition");
  JS::RootedObject constructor(cx, &args[1].toObject());
  std::unique_ptr<CustomElementDefinition> definition(new (std::nothrow)
      CustomElementDefinition(cx, std::move(name), constructor));
  if (!definition) { JS_ReportOutOfMemory(cx); return false; }
  if (!ReadDefinition(state, *definition)) return false;
  state.customElements->definitions.push_back(std::move(definition));
  JS::RootedValue defined(cx, JS::ObjectValue(*constructor));
  for (auto it = state.customElements->pending.begin(); it != state.customElements->pending.end();) {
    if ((*it)->name == state.customElements->definitions.back()->name) {
      JS::RootedObject promise(cx, (*it)->promise->get());
      if (!promise || !JS::ResolvePromise(cx, promise, defined)) return false;
      it = state.customElements->pending.erase(it);
    } else ++it;
  }
  args.rval().setUndefined();
  // Upgrade the document's existing candidates, in shadow-including tree order.
  return UpgradeTree(state, state.main, true);
}
}  // namespace

bool InstallCustomElements(State& state) {
  JSContext* cx = state.cx;
  auto registry = std::make_unique<CustomElementsRegistry>(cx);
  JS::RootedObject prototype(cx, state.Find("CustomElementRegistry")->prototype);
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, nullptr, prototype));
  if (!object) return false;
  registry->object.reset();
  registry->object.init(cx, object);
  state.customElements = std::move(registry);
  static const struct { const char* name; Operation op; unsigned length; } methods[] = {
    {"define", Define, 2}, {"get", Get, 1}, {"whenDefined", WhenDefined, 1},
    {"upgrade", Upgrade, 1}, {"getName", GetName, 1}
  };
  for (const auto& method : methods)
    if (!state.Method("CustomElementRegistry", method.name, Call, method.length, method.op)) return false;
  state.customElements->internals = JS::NewWeakMapObject(cx);
  if (!state.customElements->internals) return false;
  JS::RootedValue value(cx, JS::ObjectValue(*object));
  return JS_DefineProperty(cx, state.page, "customElements", value,
                           JSPROP_ENUMERATE | JSPROP_PERMANENT) &&
         InstallCustomStateSet(state) && InstallElementInternals(state);
}
}  // namespace ruyijs::document_model
