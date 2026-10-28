#include "web_styling/cssom.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include "runtime/runtime.h"

namespace ruyijs::web_styling {
bool CssString(JSContext* cx, JS::HandleValue value, std::string* text) {
  JS::RootedString input(cx, JS::ToString(cx, value));
  return input && ToUTF8(cx, input, text);
}
bool SheetConstruct(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  if (!args.isConstructing()) return WebKit::Fail(cx, "CSSStyleSheet requires new");
  auto cache = std::make_shared<SheetCache>(cx);
  cache->constructed = true;
  if (argc && !args[0].isNullOrUndefined()) {
    if (!args[0].isObject()) return WebKit::Fail(cx, "CSSStyleSheetInit requires a dictionary");
    JS::RootedObject options(cx, &args[0].toObject());
    JS::RootedValue value(cx);
    // WebIDL dictionary members are converted in lexicographic order.
    if (!JS_GetProperty(cx, options, "baseURL", &value)) return false;
    if (!value.isUndefined() && !CssString(cx, value, &cache->baseURL)) return false;
    if (!JS_GetProperty(cx, options, "disabled", &value)) return false;
    cache->disabled = JS::ToBoolean(value);
    if (!JS_GetProperty(cx, options, "media", &value)) return false;
    if (!value.isUndefined()) {
      JS::RootedObject media(cx, value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
      if (media && JS::GetClass(media) == &kMediaListClass) {
        JS::RootedObject source(cx, &js::GetProxyReservedSlot(media, ListSheet).toObject());
        cache->media = SheetDataOf(source).media;
      } else {
        std::string text;
        if (!CssString(cx, value, &text)) return false;
        cache->media.set(text);
      }
    }
  }
  JS::RootedObject prototype(cx),object(cx);
  if (!state.kit.ConstructProto(args, state.sheet.prototype, &prototype) ||
      !SheetObject(state, cache, &object, prototype)) return false;
  args.rval().setObject(*object); return true;
}
}  // namespace ruyijs::web_styling
