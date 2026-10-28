#include "web_platform/object_url_session.h"
#include "web_platform/object_url_store.h"
#include "web_platform/object_url_wire.h"
#include "web_platform/realm.h"
#include "web_platform/state.h"
#include "jsapi.h"
#include "js/Exception.h"
#include "js/Realm.h"
#include <unordered_map>

namespace ruyijs::web_platform {
namespace wire = object_url_wire;
struct ObjectUrlSession::Impl {
  State& state;
  std::shared_ptr<ObjectUrlStore> store;
  std::string origin;
  uint64_t owner, principal;
  std::unordered_map<uint32_t, uint64_t> references;
  Impl(State& value, std::string source)
      : state(value), store(value.objectUrls), origin(std::move(source)),
        owner(store->NewOwner()), principal(value.objectUrlOwner) {}
  void Prune() {
    for (auto it = references.begin(); it != references.end();)
      if (!store->Contains(it->second)) it = references.erase(it);
      else ++it;
  }
};
ObjectUrlSession::ObjectUrlSession(State& state, std::string origin)
    : impl_(std::make_unique<Impl>(state, std::move(origin))) {}
ObjectUrlSession::~ObjectUrlSession() { impl_->store->ClearOwner(impl_->owner); }

bool ObjectUrlSession::Request(std::string_view frame, std::string* reply) {
  auto& s = *impl_;
  JSContext* cx = s.state.cx;
  JSAutoRealm realm(cx, s.state.kit.page);
  wire::Request request;
  if (!wire::Decode(frame, &request)) return WebKit::Fail(cx, "Invalid object URL service frame");
  try {
    s.Prune();
    const std::string url(request.url);
    if (request.operation == wire::Operation::Revoke) {
      s.store->Revoke(url, s.origin, s.principal);
      s.Prune(); reply->assign(1, char(wire::Reply::Ok)); return true;
    }
    if (url.rfind("blob:" + s.origin + "/", 0) != 0)
      return WebKit::Fail(cx, "Object URL service origin mismatch");
    if (request.operation == wire::Operation::Read) {
      std::string bytes, type;
      if (!s.store->Read(url, s.origin, s.owner, &bytes, &type)) {
        JS_ClearPendingException(cx);
        reply->assign(1, char(wire::Reply::Failed));
        return true;
      }
      reply->assign(1, char(wire::Reply::Ok));
      wire::Put(*reply, uint32_t(type.size()));
      reply->append(type);
      reply->append(bytes);
      return true;
    }
    auto found = s.references.find(request.reference);
    bool ok = false;
    if (found != s.references.end()) {
      ok = s.store->RetainReference(found->second, url, s.origin, s.owner, s.principal);
    } else if (request.operation == wire::Operation::Offer) {
      reply->assign(1, char(wire::Reply::NeedBody)); return true;
    } else {
      // Allocate the transport mapping before retaining a URL, so allocation
      // failure cannot leave an unreported, permanently retained entry.
      auto slot = s.references.emplace(request.reference, 0).first;
      JS::RootedObject object(cx, MakeBlob(s.state, std::string(request.bytes),
          std::string(request.type), &kBlobClass, s.state.blob.prototype));
      if (object) ok = s.store->Retain(object, request.bytes.size(), url, s.origin,
                                      s.owner, s.principal, &slot->second);
      if (!ok) s.references.erase(slot);
    }
    if (ok) reply->assign(1, char(wire::Reply::Ok));
    else {
      const bool oom = JS_IsThrowingOutOfMemory(cx);
      JS_ClearPendingException(cx);
      reply->assign(1, char(oom ? wire::Reply::Failed : wire::Reply::Limit));
    }
    return true;
  } catch (...) { JS_ReportOutOfMemory(cx); return false; }
}

std::shared_ptr<ObjectUrlSession> Realm::OpenObjectUrlSession() {
  if (!state_->objectUrls) { WebKit::Fail(state_->cx, "Object URL catalog is not installed"); return {}; }
  std::string origin;
  if (!ObjectUrlOrigin(*state_, &origin)) return {};
  try { return std::make_shared<ObjectUrlSession>(*state_, std::move(origin)); }
  catch (...) { JS_ReportOutOfMemory(state_->cx); return {}; }
}
void Realm::SetObjectUrlExchange(ObjectUrlExchange port, void* context) {
  state_->objectUrlExchange = port;
  state_->objectUrlExchangeContext = context;
}
}
