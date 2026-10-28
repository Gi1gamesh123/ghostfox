// CookieStoreManager (Firefox 158 CookieStoreManager.webidl, Exposed=(ServiceWorker,
// Window), SecureContext): the interface behind ServiceWorkerRegistration.cookies.
// No registration here hands out a manager (D150), so the interface has no
// instances; its three Promise operations reject a foreign receiver with
// Firefox's brand error.
#include "web_service_worker/internal.h"
#include "jsapi.h"
#include "js/Object.h"
#include <iterator>

namespace ruyijs::web_service_worker {
namespace {
const JSClass kManagerClass = {"CookieStoreManager", 0};
const char* const kNames[] = {"subscribe", "getSubscriptions", "unsubscribe"};
const unsigned kLengths[] = {1, 0, 1};
bool Illegal(JSContext* cx, unsigned, JS::Value*) { return WebKit::Fail(cx, "Illegal constructor."); }
bool Member(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  (void)WebKit::Fail(cx, Incompatible(kNames[WebKit::Slot(args)], "CookieStoreManager").c_str());
  return Reject(s, args.rval());
}
}  // namespace

bool InstallCookieManager(State& s) {
  JS::RootedObject objectProto(s.cx, JS::GetRealmObjectPrototype(s.cx));
  WebFace face(s.cx);
  if (!s.kit.Define("CookieStoreManager", &kManagerClass, objectProto, Illegal, &face)) return false;
  for (int op = 0; op < int(std::size(kNames)); ++op)
    if (!s.kit.Method(face, "CookieStoreManager", kNames[op], Member, kLengths[op], op)) return false;
  return s.secure.Add(s.kit.page, "CookieStoreManager");
}
}  // namespace ruyijs::web_service_worker
