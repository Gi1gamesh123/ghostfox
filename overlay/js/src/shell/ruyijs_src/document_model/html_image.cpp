// Image's legacy factory shares the existing HTMLImageElement node owner.
#include "document_model/state.h"
#include "document_model/model_hyperlink.h"
#include "browser_environment/environment.h"
#include "runtime/runtime.h"
#include "runtime/page_helpers.h"
#include "runtime/native_constructor_trace.h"
#include "js/Conversions.h"
#include "js/CharacterEncoding.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/String.h"
#include "js/Wrapper.h"
#include <cstring>

namespace ruyijs::document_model {
namespace {
enum Field { Width, Height, Src, NaturalWidth, NaturalHeight, Complete, CurrentSrc };
const char* const kNames[] = {
    "width", "height", "src", "naturalWidth", "naturalHeight", "complete", "currentSrc"};
const char16_t* const kAttributes[] = {u"width", u"height", u"src"};
Wrapper* ImageReceiver(JSContext* cx, const JS::CallArgs& args) {
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  Wrapper* entry = object && JS::GetClass(object) == &kNodeClass
      ? static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate()) : nullptr;
  if (!entry || entry->dead || !entry->owner) {
    TypeError(cx, "HTMLImageElement member called on incompatible receiver");
    return nullptr;
  }
  State& state = *entry->owner;
  JSAutoRealm realm(cx, state.page);
  NodeSnapshot node;
  if (!state.Check(state.model.Inspect(entry->key, &node))) return nullptr;
  if (std::strcmp(node.interface_name, "HTMLImageElement")) {
    TypeError(cx, "HTMLImageElement member called on incompatible receiver");
    return nullptr;
  }
  return entry;
}
bool Dimension(State& state, NodeKey key, int field, uint32_t number) {
  std::string ascii = std::to_string(number);
  DomString text(ascii.begin(), ascii.end());
  return state.Check(state.model.SetAttribute(key, kAttributes[field], text));
}
uint32_t ReadDimension(const std::optional<DomString>& text) {
  if (!text) return 0;
  size_t i = 0;
  while (i < text->size() && ((*text)[i] == u' ' || (*text)[i] == u'\t' ||
      (*text)[i] == u'\n' || (*text)[i] == u'\r' || (*text)[i] == u'\f')) ++i;
  if (i < text->size() && (*text)[i] == u'+') ++i;
  uint32_t value = 0;
  for (; i < text->size() && (*text)[i] >= u'0' && (*text)[i] <= u'9'; ++i) {
    uint32_t digit = (*text)[i] - u'0';
    if (value > (2147483647u - digit) / 10) return 0;
    value = value * 10 + digit;
  }
  return value;
}
// Whether the attribute parses as an integer dimension (Firefox's
// nsAttrValue::eInteger for width/height): leading digits, not a percentage.
bool IntegerDimension(const std::optional<DomString>& text) {
  if (!text) return false;
  size_t i = 0;
  while (i < text->size() && ((*text)[i] == u' ' || (*text)[i] == u'\t' ||
      (*text)[i] == u'\n' || (*text)[i] == u'\r' || (*text)[i] == u'\f')) ++i;
  if (i < text->size() && (*text)[i] == u'+') ++i;
  const size_t digits = i;
  while (i < text->size() && (*text)[i] >= u'0' && (*text)[i] <= u'9') ++i;
  if (i == digits) return false;
  if (i < text->size() && (*text)[i] == u'.') {
    ++i;
    while (i < text->size() && (*text)[i] >= u'0' && (*text)[i] <= u'9') ++i;
  }
  return i == text->size() || (*text)[i] != u'%';
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = ImageReceiver(cx, args);
  if (!entry) return false;
  State& state = *entry->owner;
  int field = FunctionSlot(args);
  std::optional<DomString> raw;
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe("HTMLImageElement", kNames[field], int(ApiAccess::Read));
    if (field <= Src || field == Complete) {
      if (!state.Check(state.model.GetAttribute(entry->key,
          kAttributes[field == Complete ? Src : field], &raw))) return false;
    }
    if (field == Src && !ReflectUrl(state, entry->key, raw, args.rval())) return false;
  }
  if (field == Src) return true;
  if (field == Complete) args.rval().setBoolean(!raw || raw->empty() || entry->imageComplete);
  // Firefox's frameless GetWidthHeightForImage: an integer width/height
  // attribute, else the decoded natural size (0 before decode). Without a
  // layout engine no image has a frame, so the rendered box never applies.
  else if (field == Width)
    args.rval().setNumber(IntegerDimension(raw) ? ReadDimension(raw) : entry->imageNaturalWidth);
  else if (field == Height)
    args.rval().setNumber(IntegerDimension(raw) ? ReadDimension(raw) : entry->imageNaturalHeight);
  else if (field == NaturalWidth)
    args.rval().setNumber(entry->imageNaturalWidth);
  else if (field == NaturalHeight)
    args.rval().setNumber(entry->imageNaturalHeight);
  else
    args.rval().setString(JS_GetEmptyString(cx));  // currentSrc, as before: no selection model
  return true;
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = ImageReceiver(cx, args);
  if (!entry) return false;
  State& state = *entry->owner;
  int field = FunctionSlot(args);
  DomString text;
  uint32_t number = 0;
  if (field == Src ? !String(cx, args.get(0), &text) :
                    !JS::ToUint32(cx, args.get(0), &number)) return false;
  JSAutoRealm realm(cx, state.page);
  state.Observe("HTMLImageElement", kNames[field], int(ApiAccess::Write));
  if (field == Src ? !state.Check(state.model.SetAttribute(entry->key, u"src", text)) :
                    !Dimension(state, entry->key, field, number)) return false;
  args.rval().setUndefined();
  return true;
}
bool Image(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  State& state = *FunctionState(args);
  uint32_t dimensions[2]{};
  for (unsigned i = 0; i < 2 && i < argc; ++i)
    if (!JS::ToUint32(cx, args[i], &dimensions[i])) return false;
  Draft draft;
  if (!state.Check(state.model.Create(state.main, {NodeKind::Element, u"img", {}}, &draft))) return false;
  if (!state.Commit(draft, args.rval())) return false;
  for (unsigned i = 0; i < 2 && i < argc; ++i)
    if (!Dimension(state, draft.root, i, dimensions[i])) return false;
  return trace.Finish(true);
}
}
bool InstallImageElement(State& state) {
  for (int i = 0; i < int(std::size(kNames)); ++i)
    if (!state.Accessor("HTMLImageElement", kNames[i], Get, i, i <= Src ? Set : nullptr)) return false;
  JS::RootedObject factory(state.cx, state.Function(Image, "Image", 0, 0, JSFUN_CONSTRUCTOR));
  JS::RootedObject prototype(state.cx, state.Find("HTMLImageElement")->prototype);
  return factory && JS_DefineProperty(state.cx, factory, "prototype", prototype,
                                     JSPROP_READONLY | JSPROP_PERMANENT) &&
      JS_DefineProperty(state.cx, state.page, "Image", factory, 0) &&
      state.helpers.Constructor("Image", factory) && InstallImageMembers(state);
}
}
