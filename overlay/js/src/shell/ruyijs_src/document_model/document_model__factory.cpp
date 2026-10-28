#include "document_model/state.h"
#include "runtime/weak_object.h"
#include "interaction/realm.h"
#include "runtime/page_helpers.h"
#include "jsfriendapi.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "js/Symbol.h"
#include <new>
namespace ruyijs::document_model {
JSObject* State::Function(JSNative callback, const char* name, unsigned length,
                           int32_t slot, unsigned flags) {
  JS::RootedFunction function(cx, js::NewFunctionWithReserved(cx, callback, length, flags, name));
  if (!function) return nullptr;
  JS::RootedObject object(cx, JS_GetFunctionObject(function));
  js::SetFunctionNativeReserved(object, 0, JS::PrivateValue(this));
  js::SetFunctionNativeReserved(object, 1, JS::Int32Value(slot));
  if (!functions.get().append(object)) { JS_ReportOutOfMemory(cx); return nullptr; }
  return object;
}
bool State::Method(const char* iface, const char* name, JSNative callback,
                   unsigned length, int32_t slot) {
  JS::RootedObject target(cx, Find(iface)->prototype);
  JS::RootedObject method(cx, Function(callback, name, length, slot));
  return method && JS_DefineProperty(cx, target, name, method, JSPROP_ENUMERATE);
}
bool State::Accessor(const char* iface, const char* name, JSNative getter,
                     int32_t slot, JSNative setter) {
  JS::RootedObject target(cx, Find(iface)->prototype);
  std::string getName = std::string("get ") + name;
  JS::RootedObject get(cx, Function(getter, getName.c_str(), 0, slot));
  JS::RootedObject set(cx);
  if (setter) {
    std::string setName = std::string("set ") + name;
    set = Function(setter, setName.c_str(), 1, slot);
    if (!set) return false;
  }
  return get && JS_DefineProperty(cx, target, name, get, set, JSPROP_ENUMERATE);
}
bool State::DefineInterface(const char* name, JS::HandleObject parent, int32_t kind,
                            bool inherit_constructor) {
  std::unique_ptr<Interface> entry(new (std::nothrow) Interface(cx, name));
  if (!entry) { JS_ReportOutOfMemory(cx); return false; }
  entry->prototype = JS_NewPlainObject(cx);
  entry->constructor = Function(Constructor, name, kind == NewStaticRange ? 1 : 0, kind, JSFUN_CONSTRUCTOR);
  if (!entry->prototype || !entry->constructor ||
      !JS_SetPrototype(cx, entry->prototype, parent)) return false;
  // Resolve native function keys before defining the permanent prototype key.
  JS::RootedValue metadata(cx);
  if (!JS_GetProperty(cx, entry->constructor, "length", &metadata) ||
      !JS_GetProperty(cx, entry->constructor, "name", &metadata)) return false;
  if (inherit_constructor) {
    JS::RootedValue ancestor(cx);
    if (!JS_GetProperty(cx, parent, "constructor", &ancestor)) return false;
    if (ancestor.isObject()) {
      JS::RootedObject object(cx, &ancestor.toObject());
      if (!JS_SetPrototype(cx, entry->constructor, object)) return false;
    }
  }
  JS::RootedString tag(cx, JS_NewStringCopyZ(cx, name));
  JS::RootedId id(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::toStringTag)));
  if (!tag || !JS_DefineProperty(cx, entry->constructor, "prototype", entry->prototype,
                                 JSPROP_READONLY | JSPROP_PERMANENT) ||
      !JS_DefineProperty(cx, entry->prototype, "constructor", entry->constructor, 0) ||
      !JS_DefinePropertyById(cx, entry->prototype, id, tag, JSPROP_READONLY) ||
      !JS_DefineProperty(cx, page, name, entry->constructor, 0)) return false;
  entry->next = std::move(interfaces);
  interfaces = std::move(entry);
  return true;
}
bool State::TraceInterface(const char* name) {
  Interface* entry = Find(name);
  return helpers.Interface(name, entry->prototype) && helpers.Constructor(name, entry->constructor);
}
void State::Rekey(const std::vector<std::pair<NodeKey, NodeKey>>& pairs) {
  if (pairs.empty()) { FlushMutationNotification(); return; }
  model.RekeyDocumentReferences(pairs);
  for (const auto& [from, to] : pairs) {
    for (Wrapper* entry = wrappers.get(); entry; entry = entry->next.get())
      if (Same(entry->key, from)) {
        // A dying wrapper follows the key so its reclaim releases the right
        // node, but never displaces the replacement that owns the index (T41).
        const bool live = !entry->dead && WeakPeek(entry->object);
        if (live) wrapperIndex.erase(entry->key);
        entry->key = to;
        if (live) wrapperIndex[to] = entry;
      }
    CustomElementsRekey(*this, from, to);
    for (ReflectedLink* entry = reflected.get(); entry; entry = entry->next.get())
      if (Same(entry->key, from)) entry->key = to;
    for (FrameLink* entry = frames.get(); entry; entry = entry->next.get())
      if (Same(entry->frame, from)) entry->frame = to;
    for (auto* record = tokenViews; record; record = record->weakNext) {
      if (Same(record->scope, from)) record->scope = to;
      record->revision = 0;
    }
    for (Collection* entry = collections.get(); entry; entry = entry->next.get()) {
      CollectionRecord& record = *entry->record;
      if (Same(record.scope, from)) record.scope = to;
      for (NodeKey& item : record.items)
        if (Same(item, from)) item = to;
      // Force the next owner-revision sync to rebuild the keys from the tree.
      record.revision = 0;
    }
  }
  // The adoption is a membership change, so every live collection must re-collect
  // even though no wrapper was created or destroyed.
  syncedRevision = 0;
  // Native consumers update their original keys before matching the pending
  // mutation log. The same mapping also covers nodes without live wrappers.
  NotifyNodeLifecycle({NodeLifecycleKind::Rekeyed, pairs, {}});
  // Consumers may now capture the original objects from the committed keys.
  // Publish before style/frame discovery can itself allocate or change trees.
  FlushMutationNotification();
  // The transfer already committed; every original wrapper key must be updated
  // before asking its existing CSS associations to inspect the new document.
  for (const auto& pair : pairs) {
    Wrapper* entry = Find(pair.second);
    if (entry && !NotifyStyleAssociations(*this, *entry, true)) return;
  }
  auto deferred=std::move(deferredStyleScopes);
  deferredStyleScopes.clear();
  for (NodeKey& scope:deferred) {
    for (const auto& [from,to]:pairs) if (Same(scope,from)) { scope=to;break; }
  }
  deferred.push_back(pairs.front().second);
  // Old associations were retired at commit and during the identity update
  // above. Replay only membership/discovery: reading one source may create
  // another source's sheet in the same scope, which must survive this replay.
  for (size_t i=0;i<deferred.size();++i) {
    bool seen=false;
    for (size_t j=0;j<i;++j) if (Same(deferred[i],deferred[j])) { seen=true;break; }
    if (!seen) SyncMembership(deferred[i],false);
  }
  FlushFrameLifecycle();
  FlushScriptChanges();
  FlushCustomElementChanges(*this);
}


}
