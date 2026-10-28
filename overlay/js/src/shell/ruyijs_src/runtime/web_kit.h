#ifndef RUYIJS_WEB_KIT_H
#define RUYIJS_WEB_KIT_H
#include <cstdint>
#include <string>
#include "js/CallArgs.h"
#include "js/GCVector.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs {
class PageHelpers;
struct WebFace {
  const JSClass* clasp = nullptr;
  JS::PersistentRootedObject prototype, constructor;
  explicit WebFace(JSContext* cx) : prototype(cx), constructor(cx) {}
};
class WebKit {
 public:
  WebKit(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, void* owner);
  bool Define(const char* name, const JSClass* clasp, JS::HandleObject parent,
              JSNative ctor, WebFace* face, unsigned ctorLength = 0);
  bool Method(WebFace& face, const char* iface, const char* name, JSNative native,
              unsigned length, int32_t slot);
  bool Accessor(WebFace& face, const char* iface, const char* name, JSNative getter,
                JSNative setter, int32_t slot);
  JSObject* Function(JSNative native, const char* name, unsigned length, int32_t slot,
                     unsigned flags = 0);
  bool Resolve(JS::HandleValue value, JS::MutableHandleValue result);
  // A rejected promise over the caller's reason, for state machines that must
  // reject rather than throw (Web Audio context transitions).
  bool RejectWith(JS::HandleValue reason, JS::MutableHandleValue result);
  JSObject* New(const JSClass* clasp, JS::HandleObject proto);
  bool ConstructProto(const JS::CallArgs& args, JS::HandleObject fallback,
                      JS::MutableHandleObject proto);
  static bool Fail(JSContext* cx, const char* message);
  // Same as Fail but with an explicit error kind, e.g. JSEXN_RANGEERR.
  static bool FailKind(JSContext* cx, const char* message, int16_t kind);
  static bool Reject(JSContext* cx, unsigned argc, JS::Value* vp);
  static void* Owner(const JS::CallArgs& args);
  static int32_t Slot(const JS::CallArgs& args);
  // A WebIDL brand failure in Firefox's words: "'<member>' called on an object
  // that does not implement interface <iface>.", the member being the callee's
  // own name with an accessor's "get "/"set " prefix. BrandText only builds it.
  static bool BrandText(JSContext* cx, const JS::CallArgs& args, const char* iface,
                        std::string* text);
  static bool FailBrand(JSContext* cx, const JS::CallArgs& args, const char* iface);
  static bool Check(JSContext* cx, JS::HandleValue self, const JSClass* clasp,
                    JS::MutableHandleObject out, const char* message);
  // Brand check whose failure reports Firefox's exact WebIDL wording
  // ("'<member>' called on an object that does not implement interface
  // <iface>.") built from the callee via BrandText, instead of a fixed
  // string. Cloudflare's Turnstile fingerprints these messages per method, so
  // the receiver checks on fingerprinted interfaces (Canvas, WebGL, Crypto,
  // Performance, ...) must match Firefox verbatim.
  static bool CheckBrand(JSContext* cx, const JS::CallArgs& args, const JSClass* clasp,
                         JS::MutableHandleObject out, const char* iface);
  JSContext* cx;
  PageHelpers* helpers;
  void* owner;
  JS::PersistentRootedObject page;
  JS::PersistentRooted<JS::GCVector<JSObject*>> functions;
};
bool JsonText(JSContext* cx, JS::HandleValue value, std::string* utf8);
bool JsonValue(JSContext* cx, const std::string& utf8, JS::MutableHandleValue value);
}
#endif
