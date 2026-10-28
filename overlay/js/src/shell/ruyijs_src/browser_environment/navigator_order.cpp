// Navigator.prototype key order. Several owners append their Navigator member
// (Permissions, Geolocation, WebGPU, Media, Storage, ...), each when its realm
// installs, so definition order alone cannot reproduce the order Firefox's
// generated NavigatorBinding defines them in. After every installer has run,
// the runtime coordinator calls OrderNavigator once; it re-defines each
// configurable own property, unchanged, in the binding order below. Members a
// realm did not install are skipped, unknown keys keep their relative order at
// the end, and "constructor" stays last as in the binding (runtime/binding_order.h).
// A Worker realm orders WorkerNavigator by its own binding.
#include "browser_environment/environment.h"
#include "browser_environment/state.h"
#include "runtime/binding_order.h"
#include <iterator>

namespace ruyijs::browser_environment {
namespace {
// Firefox 158 Navigator.prototype own keys (domtrace
// jscall-detail-desktop-20261019-230430 and the window surface probe).
const char* const kBindingOrder[] = {
    "javaEnabled", "getGamepads", "requestMIDIAccess", "mozGetUserMedia", "sendBeacon",
    "requestMediaKeySystemAccess", "getAutoplayPolicy", "registerProtocolHandler",
    "taintEnabled", "permissions", "mimeTypes", "plugins", "pdfViewerEnabled", "doNotTrack",
    "maxTouchPoints", "mediaCapabilities", "oscpu", "vendor", "vendorSub", "productSub",
    "cookieEnabled", "buildID", "mediaDevices", "serviceWorker", "credentials", "clipboard",
    "mediaSession", "userActivation", "wakeLock", "login", "globalPrivacyControl", "webdriver",
    "hardwareConcurrency", "gpu", "geolocation", "appCodeName", "appName", "appVersion",
    "platform", "userAgent", "product", "language", "languages", "locks", "onLine", "storage"};

// Firefox 158 WorkerNavigator.prototype own keys (live dedicated Worker probe,
// artifacts/worker-parity/firefox-worker.json, 2026-10-21).
const char* const kWorkerBindingOrder[] = {
    "mediaCapabilities", "permissions", "serviceWorker", "globalPrivacyControl", "hardwareConcurrency",
    "gpu", "appCodeName", "appName", "appVersion", "platform", "userAgent", "product", "language",
    "languages", "locks", "onLine", "storage"};
}  // namespace

bool Environment::OrderNavigator() {
  JS::RootedObject prototype(state_->cx, state_->navigatorPrototype);
  if (!prototype) return false;
  return state_->worker
      ? OrderOwnKeys(state_->cx, prototype, kWorkerBindingOrder, std::size(kWorkerBindingOrder))
      : OrderOwnKeys(state_->cx, prototype, kBindingOrder, std::size(kBindingOrder));
}
}  // namespace ruyijs::browser_environment
