// The one production installer of ClipboardItem (dom/webidl/Clipboard.webidl):
// interface object, prototype members in the donor's order, and supports().
#include "web_clipboard/internal.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::web_clipboard {
Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers)
    : state_(std::make_unique<State>(cx, page, helpers)) {}
Realm::~Realm() = default;
void Realm::SetBlobBrandPort(BlobBrandPort port) { state_->blobBrand = port; }
bool Realm::InstallNavigatorClipboard(JS::HandleObject eventTarget, JS::HandleObject navigatorPrototype,
                                      AttachPort attach, void* context) {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.kit.page);
  s.attach = attach;
  s.attachContext = context;
  return InstallClipboard(s, eventTarget, navigatorPrototype);
}
bool Realm::ConfigureExposure() {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.kit.page);
  JS::RootedValue secure(s.cx);
  return JS_GetProperty(s.cx, s.kit.page, "isSecureContext", &secure) && s.secure.Apply(secure.isTrue());
}

bool Realm::Install() {
  State& s = *state_;
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.kit.page);
  // getType() settles with this realm's Blob and DOMException, captured once
  // here so a page reassigning those globals cannot change the results.
  JS::RootedValue blob(cx), exception(cx);
  if (!JS_GetProperty(cx, s.kit.page, "Blob", &blob) ||
      !JS_GetProperty(cx, s.kit.page, "DOMException", &exception)) return false;
  if (!blob.isObject() || !exception.isObject())
    return WebKit::Fail(cx, "ClipboardItem requires the realm's Blob and DOMException");
  s.blob = &blob.toObject();
  s.exception = &exception.toObject();
  JS::RootedObject objectPrototype(cx, JS::GetRealmObjectPrototype(cx));
  WebFace& face = s.face;
  if (!objectPrototype ||
      !s.kit.Define("ClipboardItem", &kItemClass, objectPrototype, Construct, &face) ||
      !JS_DefineProperty(cx, face.constructor, "length", 1, JSPROP_READONLY) ||
      !s.kit.Method(face, "ClipboardItem", "getType", GetType, 1, 0) ||
      !s.kit.Accessor(face, "ClipboardItem", "presentationStyle", StyleGet, nullptr, 0) ||
      !s.kit.Accessor(face, "ClipboardItem", "types", TypesGet, nullptr, 0))
    return false;
  JS::RootedObject supports(cx, s.kit.Function(Supports, "supports", 1, 0));
  JS::RootedValue constructor(cx, JS::ObjectValue(*face.constructor));
  return supports &&
         JS_DefineProperty(cx, face.constructor, "supports", supports, JSPROP_ENUMERATE) &&
         s.kit.helpers->Member(supports, "ClipboardItem", "supports", "call") &&
         JS_DeleteProperty(cx, face.prototype, "constructor") &&
         JS_DefineProperty(cx, face.prototype, "constructor", constructor, 0);
}
}  // namespace ruyijs::web_clipboard
