#include "web_styling/cssom.h"
#include "js/Realm.h"

namespace ruyijs::web_styling {
bool SheetLegacy(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  bool adding = WebKit::Slot(args) != 0;
  JS::RootedObject self(cx);
  if (!CssReceiver(cx, args.thisv(), &kSheetClass, &self)) return false;
  uint32_t index = 0;
  if (adding) {
    std::string selector = "undefined", style = "undefined";
    if (argc > 0 && !args[0].isUndefined() && !CssString(cx, args[0], &selector)) return false;
    if (argc > 1 && !args[1].isUndefined() && !CssString(cx, args[1], &style)) return false;
    index = uint32_t(SheetDataOf(self).rules.size());
    if (argc > 2 && !args[2].isUndefined() && !JS::ToUint32(cx, args[2], &index)) return false;
    if (!InsertCssRule(cx, self, selector + "{" + style + "}", index)) return false;
    args.rval().setInt32(-1);
  } else {
    if (argc && !JS::ToUint32(cx, args[0], &index)) return false;
    if (!DeleteCssRule(cx, self, index)) return false;
    args.rval().setUndefined();
  }
  return true;
}
}  // namespace ruyijs::web_styling
