// Public ruyijs's decode placeholder examines the original src content attribute.
#include "document_model/html_control.h"
#include "js/ErrorReport.h"
#include "js/Exception.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
bool ImageDecode(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* entry = ControlReceiver(cx, args.thisv(), "HTMLImageElement");
  if (!entry) return false;
  auto& state = *entry->owner;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe("HTMLImageElement", "decode", int(ApiAccess::Call));
    std::optional<DomString> src;
    if (!state.Check(state.model.GetAttribute(entry->key, u"src", &src))) return false;
    JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
    if (!promise) return false;
    if (src && !src->empty()) {
      if (!JS::ResolvePromise(cx, promise, JS::UndefinedHandleValue)) return false;
    } else {
      JS::RootedString message(cx, JS_NewStringCopyZ(cx, "The source image cannot be decoded"));
      JS::RootedString file(cx, JS_NewStringCopyZ(cx, "ruyijs:web"));
      JS::Rooted<mozilla::Maybe<JS::Value>> cause(cx, mozilla::Nothing());
      JS::RootedValue error(cx);
      if (!message || !file || !JS::CreateError(cx, JSEXN_ERR, nullptr, file, 1,
          JS::ColumnNumberOneOrigin(), nullptr, message, cause, &error) ||
          !JS::RejectPromise(cx, promise, error)) return false;
    }
    result.setObject(*promise);
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
}
