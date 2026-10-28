#include "web_styling/cssom.h"
#include "js/Realm.h"
#include "js/Symbol.h"

namespace ruyijs::web_styling {
bool NewCssList(State& state, JS::HandleObject sheet, JS::MutableHandleObject out, CssListKind kind,
                 JS::HandleObject document) {
  JSContext* cx = state.cx;
  if (!sheet && kind != MediaCollection && !document)
    return WebKit::Fail(cx, "StyleSheetList requires its original Document");
  JS::RootedObject expando(cx, JS_NewObjectWithGivenProto(cx, nullptr, nullptr));
  if (!expando) return false;
  JS::RootedValue value(cx, JS::ObjectValue(*expando));
  js::ProxyOptions options;
  bool media = kind == MediaCollection;
  options.setClass(media ? &kMediaListClass : sheet ? &kRuleListClass : &kSheetListClass);
  JS::RootedObject prototype(cx, media ? state.mediaList.prototype.get() :
      sheet ? state.ruleList.prototype.get() : state.sheetList.prototype.get());
  out.set(js::NewProxyObject(cx, CssListHandler(), value, prototype, options));
  if (!out) return false;
  js::SetProxyReservedSlot(out, ListOwner, JS::PrivateValue(&state));
  js::SetProxyReservedSlot(out, ListSheet, sheet ? JS::ObjectValue(*sheet) : JS::NullValue());
  if (!media && !sheet) js::SetProxyReservedSlot(out, ListDocument, JS::ObjectValue(*document));
  return true;
}
bool CssListLength(JSContext* cx, JS::HandleObject list, uint32_t* length) {
  if(JS::GetClass(list)==&kKeyframesRuleClass) {
    *length=uint32_t(RuleDataOf(list).children.size());return true;
  }
  if (JS::GetClass(list) == &kRuleListClass || JS::GetClass(list) == &kMediaListClass) {
    JS::RootedObject sheet(cx, &js::GetProxyReservedSlot(list, ListSheet).toObject());
    *length = uint32_t(JS::GetClass(list) == &kMediaListClass ?
        ContainerMedia(sheet).entries().size() : ContainerRows(sheet).size());
    return true;
  }
  State& state = ListOwnerOf(list);
  JS::RootedObject document(cx, &js::GetProxyReservedSlot(list, ListDocument).toObject());
  JSAutoRealm realm(cx, state.page);
  std::vector<document_model::StyleSheetSource> sources;
  if (!ReadDocumentSheetSources(state, document, &sources)) return false;
  *length = uint32_t(sources.size());
  return true;
}
bool CssListAt(JSContext* cx, JS::HandleObject list, uint32_t index,
                JS::MutableHandleValue result) {
  const bool keyframes=JS::GetClass(list)==&kKeyframesRuleClass;
  State& state = keyframes?RuleOwnerOf(list):ListOwnerOf(list);
  {
    JSAutoRealm realm(cx, state.page);
    JS::RootedObject object(cx);
    if (JS::GetClass(list) == &kMediaListClass) {
      JS::RootedObject sheet(cx, &js::GetProxyReservedSlot(list, ListSheet).toObject());
      const auto& rows = ContainerMedia(sheet).entries();
      if (index < rows.size()) {
        JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(rows[index].data(), rows[index].size())));
        if (!text) return false;
        result.setString(text);
      } else result.setUndefined();
    } else if (JS::GetClass(list) == &kRuleListClass || keyframes) {
      JS::RootedObject sheet(cx, keyframes?list.get():&js::GetProxyReservedSlot(list, ListSheet).toObject());
      if (index < ContainerRows(sheet).size() &&
          !detail::NewCssRule(state, sheet, index, &object)) return false;
    } else {
      JS::RootedObject document(cx, &js::GetProxyReservedSlot(list, ListDocument).toObject());
      if (!SheetAt(state, index, &object, document)) return false;
    }
    if (JS::GetClass(list) != &kMediaListClass) {
      if (object) result.setObject(*object); else result.setUndefined();
    }
  }
  return JS_WrapValue(cx, result);
}
namespace {
bool Member(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int member = WebKit::Slot(args);
  const JSClass* clasp = member < 2 ? &kRuleListClass : &kSheetListClass;
  JS::RootedObject self(cx);
  if (!CssReceiver(cx, args.thisv(), clasp, &self)) return false;
  if (member % 2 == 0) {
    uint32_t length;
    if (!CssListLength(cx, self, &length)) return false;
    args.rval().setNumber(double(length));
    return true;
  }
  if (!argc) return WebKit::Fail(cx, "CSSOM list.item requires an index");
  uint32_t index;
  if (!JS::ToUint32(cx, args[0], &index) || !CssListAt(cx, self, index, args.rval())) return false;
  if (args.rval().isUndefined()) args.rval().setNull();
  return true;
}
bool InstallList(State& state, bool rules) {
  WebKit& kit = state.kit;
  WebFace& face = rules ? state.ruleList : state.sheetList;
  const char* name = rules ? "CSSRuleList" : "StyleSheetList";
  JS::RootedObject plain(kit.cx, JS::GetRealmObjectPrototype(kit.cx));
  if (!kit.Define(name, rules ? &kRuleListClass : &kSheetListClass, plain,
                  WebKit::Reject, &face)) return false;
  JS::ObjectOpResult deleted;
  if (!JS_DeleteProperty(kit.cx, face.prototype, "constructor", deleted) ||
      !kit.Method(face, name, "item", Member, 1, rules ? 1 : 3) ||
      !kit.Accessor(face, name, "length", Member, nullptr, rules ? 0 : 2) ||
      !JS_DefineProperty(kit.cx, face.prototype, "constructor", face.constructor, 0)) return false;
  JS::RootedObject array(kit.cx, JS::GetRealmArrayPrototype(kit.cx));
  JS::RootedValue values(kit.cx);
  JS::RootedId id(kit.cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(kit.cx, JS::SymbolCode::iterator)));
  return array && JS_GetProperty(kit.cx, array, "values", &values) &&
         JS_DefinePropertyById(kit.cx, face.prototype, id, values, 0);
}
}
bool InstallRuleList(State& state) { return InstallList(state, true); }
bool InstallSheetList(State& state) { return InstallList(state, false); }
}  // namespace ruyijs::web_styling
