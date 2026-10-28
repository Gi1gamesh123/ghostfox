#include "web_platform/object_url_store.h"
#include "web_platform/state.h"
#include "domain/web_platform/object_url_catalog.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/Realm.h"
#include <unordered_map>

namespace ruyijs::web_platform {
using Catalog = ruyijs::domain::web_platform::ObjectUrlCatalog;
struct ObjectUrlStore::Impl {
  JSContext* cx;
  Catalog catalog;
  uint64_t nextOwner = 0, nextReference = 0;
  std::unordered_map<uint64_t, std::unique_ptr<JS::PersistentRootedObject>> roots;
  explicit Impl(JSContext* context) : cx(context) {}
  void Prune() {
    for (auto it = roots.begin(); it != roots.end();) {
      if (!catalog.Retained(it->first)) it = roots.erase(it);
      else ++it;
    }
  }
};
ObjectUrlStore::ObjectUrlStore(JSContext* cx) : impl_(std::make_unique<Impl>(cx)) {}
ObjectUrlStore::~ObjectUrlStore() = default;
uint64_t ObjectUrlStore::NewOwner() { return ++impl_->nextOwner; }
bool ObjectUrlStore::Retain(JS::HandleObject object, size_t bytes, const std::string& url,
                             const std::string& origin, uint64_t owner, uint64_t principal,
                             uint64_t* retained) {
  auto& s = *impl_;
  try {
    uint64_t reference = 0;
    // A moving GC updates each root. Hashing raw object addresses would instead
    // leave stale keys; this bounded scan never crosses an engine allocation.
    for (const auto& item : s.roots)
      if (item.second->get() == object) { reference = item.first; break; }
    if (!reference) {
      reference = ++s.nextReference;
      s.roots.emplace(reference, std::make_unique<JS::PersistentRootedObject>(s.cx, object));
    }
    const auto added = s.catalog.Add(url, origin, owner, reference, bytes, principal);
    if (added != Catalog::Added::Ok) {
      s.Prune();
      return WebKit::FailKind(s.cx, "Object URL retention limit exceeded", JSEXN_RANGEERR);
    }
    if (retained) *retained = reference;
    return true;
  } catch (...) {
    s.Prune();
    JS_ReportOutOfMemory(s.cx);
    return false;
  }
}
bool ObjectUrlStore::Contains(uint64_t reference) const {
  return impl_->catalog.Retained(reference);
}
bool ObjectUrlStore::RetainReference(uint64_t reference, const std::string& url,
                                     const std::string& origin, uint64_t owner, uint64_t principal) {
  auto& s = *impl_;
  if (!s.catalog.Retained(reference)) return WebKit::Fail(s.cx, "Object URL reference was released");
  try {
    if (s.catalog.Add(url, origin, owner, reference, 0, principal) == Catalog::Added::Ok) return true;
    return WebKit::FailKind(s.cx, "Object URL retention limit exceeded", JSEXN_RANGEERR);
  } catch (...) { JS_ReportOutOfMemory(s.cx); return false; }
}
bool ObjectUrlStore::Read(const std::string& url, const std::string& origin, uint64_t owner,
                           std::string* bytes, std::string* type) {
  auto& s = *impl_;
  try {
    const auto reference = s.catalog.Read(url, origin, owner);
    auto found = s.roots.find(reference);
    if (found == s.roots.end()) return false;
    JS::RootedObject object(s.cx, found->second->get());
    JSAutoRealm realm(s.cx, object);
    const auto* shape = JS::GetClass(object);
    if (shape != &kBlobClass && shape != &kFileClass) return false;
    JS::RootedValue field(s.cx, JS::GetReservedSlot(object, kBlobTypeSlot));
    if (!Convert(s.cx, field, type)) return false;
    const uint8_t* data = nullptr;
    size_t length = 0;
    if (!BlobBytes(s.cx, object, &data, &length)) return false;
    if (length) bytes->assign(reinterpret_cast<const char*>(data), length);
    else bytes->clear();
    return true;
  } catch (...) { JS_ReportOutOfMemory(s.cx); return false; }
}
void ObjectUrlStore::Revoke(const std::string& url, const std::string& origin, uint64_t owner) {
  impl_->catalog.Revoke(url, origin, owner);
  impl_->Prune();
}
void ObjectUrlStore::ClearOwner(uint64_t owner) {
  impl_->catalog.ClearOwner(owner);
  impl_->Prune();
}
}
