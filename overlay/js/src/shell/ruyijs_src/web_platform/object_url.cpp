#include "web_platform/state.h"
#include "web_platform/object_url_store.h"
#include "web_platform/object_url_remote.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#endif

namespace ruyijs::web_platform {
namespace {
bool Uuid(JSContext* cx, std::string* result) {
  unsigned char bytes[16];
#ifdef _WIN32
  if (BCryptGenRandom(nullptr, bytes, sizeof(bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
    return WebKit::Fail(cx, "Object URL random source failed");
#else
  FILE* urandom = fopen("/dev/urandom", "rb");
  if (!urandom || fread(bytes, 1, sizeof(bytes), urandom) != sizeof(bytes)) {
    if (urandom) fclose(urandom);
    return WebKit::Fail(cx, "Object URL random source failed");
  }
  fclose(urandom);
#endif
  bytes[6] = (bytes[6] & 15) | 64;
  bytes[8] = (bytes[8] & 63) | 128;
  constexpr char hex[] = "0123456789abcdef";
  for (size_t i = 0; i < sizeof(bytes); ++i) {
    if (i == 4 || i == 6 || i == 8 || i == 10) result->push_back('-');
    result->push_back(hex[bytes[i] >> 4]); result->push_back(hex[bytes[i] & 15]);
  }
  return true;
}
}
bool ObjectUrlOrigin(State& state, std::string* result) {
  if (!state.objectUrlOrigin) return WebKit::Fail(state.cx, "Object URL origin owner is not bound");
  return state.objectUrlOrigin(state.objectUrlOriginContext, result);
}
bool CreateObjectUrl(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject object(cx, argc && args[0].isObject()
      ? js::CheckedUnwrapStatic(&args[0].toObject()) : nullptr);
  if (!object) return WebKit::Fail(cx, "URL.createObjectURL requires a Blob or MediaSource");
  size_t bytes = 0;
  {
    JSAutoRealm realm(cx, object);
    const auto* shape = JS::GetClass(object);
    if (shape == &kBlobClass || shape == &kFileClass) {
      const uint8_t* data = nullptr;
      if (!BlobBytes(cx, object, &data, &bytes)) return false;
    } else if (!state.objectUrlMediaBrand || !state.objectUrlMediaBrand(object)) {
      return WebKit::Fail(cx, "URL.createObjectURL requires a Blob or MediaSource");
    }
  }
  std::string origin, url;
  if (!ObjectUrlOrigin(state, &origin)) return false;
  url = "blob:" + origin + "/";
  if (!Uuid(cx, &url) || !Value(cx, url, args.rval())) return false;
  if (state.objectUrlExchange) return RetainRemoteObjectUrl(state, object, url);
  return state.objectUrls->Retain(object, bytes, url, origin, state.objectUrlOwner);
}
bool RevokeObjectUrl(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto& state = *static_cast<State*>(WebKit::Owner(args));
  if (!argc) return WebKit::Fail(cx, "URL.revokeObjectURL requires a URL");
  std::string url, origin;
  if (!Convert(cx, args[0], &url) || !ObjectUrlOrigin(state, &origin)) return false;
  if (state.objectUrlExchange) {
    if (!RevokeRemoteObjectUrl(state, url)) return false;
  } else state.objectUrls->Revoke(url, origin, state.objectUrlOwner);
  args.rval().setUndefined();
  return true;
}
}
