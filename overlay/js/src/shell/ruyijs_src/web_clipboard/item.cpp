// The ClipboardItem constructor, its two attributes and supports(). Argument
// conversion follows the donor binding: the record and the options dictionary
// are converted first, then ClipboardItem::Constructor validates the entries.
#include "web_clipboard/internal.h"
#include "domain/clipboard/clipboard_type.h"
#include "runtime/native_constructor_trace.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/ColumnNumber.h"
#include "js/ErrorReport.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
#include "mozilla/Range.h"
#include <vector>

namespace ruyijs::web_clipboard {
using domain::clipboard::ParseClipboardType;
const JSClass kItemClass = {"ClipboardItem", JSCLASS_HAS_RESERVED_SLOTS(ItemSlotCount)};
const JSClass kEntryClass = {"ClipboardItemEntry", JSCLASS_HAS_RESERVED_SLOTS(EntrySlotCount)};

bool ToUtf16(JSContext* cx, JS::HandleValue value, std::u16string* out) {
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text) return false;
  out->resize(JS_GetStringLength(text));
  return JS_CopyStringChars(cx, mozilla::Range<char16_t>(out->data(), out->size()), text);
}
JSString* NewString(JSContext* cx, const std::u16string& text) {
  return JS_NewUCStringCopyN(cx, text.data(), text.size());
}
bool ThrowType(JSContext* cx, const std::u16string& message) {
  JS::RootedString text(cx, NewString(cx, message));
  JS::RootedString file(cx, JS_NewStringCopyZ(cx, "ruyijs:web"));
  JS::Rooted<mozilla::Maybe<JS::Value>> cause(cx, mozilla::Nothing());
  JS::RootedValue error(cx);
  if (!text || !file || !JS::CreateError(cx, JSEXN_TYPEERR, nullptr, file, 1,
                                         JS::ColumnNumberOneOrigin(), nullptr,
                                         text, cause, &error)) return false;
  JS_SetPendingException(cx, error);
  return false;
}

JSObject* ItemReceiver(JSContext* cx, JS::HandleValue self, const char* member) {
  JSObject* object = self.isObject() ? js::CheckedUnwrapStatic(&self.toObject()) : nullptr;
  if (object && JS::GetClass(object) == &kItemClass) return object;
  WebKit::Fail(cx, (std::string("'") + member +
      "' called on an object that does not implement interface ClipboardItem.").c_str());
  return nullptr;
}

namespace {
constexpr const char16_t* kPrefix = u"ClipboardItem constructor: ";
// ClipboardItemOptions.presentationStyle, defaulting to "unspecified".
bool Style(JSContext* cx, JS::HandleValue options, std::u16string* style) {
  *style = u"unspecified";
  if (options.isNullOrUndefined()) return true;
  if (!options.isObject())
    return ThrowType(cx, std::u16string(kPrefix) + u"Argument 2 can't be converted to a dictionary.");
  JS::RootedObject bag(cx, &options.toObject());
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, bag, "presentationStyle", &value)) return false;
  if (value.isUndefined()) return true;
  if (!ToUtf16(cx, value, style)) return false;
  for (const char16_t* known : {u"unspecified", u"inline", u"attachment"})
    if (*style == known) return true;
  return ThrowType(cx, std::u16string(kPrefix) + u"'" + *style +
      u"' (value of 'presentationStyle' member of ClipboardItemOptions) is not a valid value "
      u"for enumeration PresentationStyle.");
}
}  // namespace

bool Construct(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing()) return ThrowType(cx, std::u16string(kPrefix) + u"'new' is required");
  if (!argc) return ThrowType(cx, std::u16string(kPrefix) + u"At least 1 argument required, but only 0 passed");
  if (!args[0].isObject()) return ThrowType(cx, std::u16string(kPrefix) + u"Argument 1 is not an object.");
  // record<DOMString, Promise<(DOMString or Blob)>>: every own enumerable key
  // (a symbol key fails its DOMString conversion, as in the donor binding),
  // each value converted with this realm's original Promise.resolve.
  JS::RootedObject record(cx, &args[0].toObject());
  JS::RootedIdVector ids(cx);
  if (!js::GetPropertyKeys(cx, record, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS, &ids)) return false;
  std::vector<std::u16string> keys;
  JS::RootedObject promises(cx, JS::NewArrayObject(cx, 0));
  if (!promises) return false;
  for (size_t i = 0; i < ids.length(); ++i) {
    JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> descriptor(cx);
    if (!JS_GetOwnPropertyDescriptorById(cx, record, ids[i], &descriptor)) return false;
    if (descriptor.isNothing() || !descriptor->enumerable()) continue;
    JS::RootedValue key(cx), value(cx);
    std::u16string text;
    if (!JS_IdToValue(cx, ids[i], &key) || !ToUtf16(cx, key, &text) ||
        !JS_GetPropertyById(cx, record, ids[i], &value)) return false;
    JS::RootedObject promise(cx, JS::CallOriginalPromiseResolve(cx, value));
    JS::RootedValue held(cx, JS::ObjectValue(*promise));
    if (!promise || !JS_SetElement(cx, promises, keys.size(), held)) return false;
    keys.push_back(text);
  }
  std::u16string style;
  if (!Style(cx, args.get(1), &style)) return false;
  if (keys.empty()) return ThrowType(cx, std::u16string(kPrefix) + u"At least one entry required");
  JS::RootedObject proto(cx);
  if (!state.kit.ConstructProto(args, state.face.prototype, &proto)) return false;
  JS::RootedObject item(cx, state.kit.New(&kItemClass, proto));
  JS::RootedObject entries(cx, JS::NewArrayObject(cx, 0)), types(cx, JS::NewArrayObject(cx, 0));
  JS::RootedString styleText(cx, NewString(cx, style));
  if (!item || !entries || !types || !styleText) return false;
  std::vector<std::u16string> seen;
  for (size_t i = 0; i < keys.size(); ++i) {
    auto parsed = ParseClipboardType(keys[i]);
    if (!parsed.valid)
      return ThrowType(cx, std::u16string(kPrefix) + u"Type '" + keys[i] + u"' is not a valid type");
    for (const auto& type : seen)
      if (type == parsed.type)
        return ThrowType(cx, std::u16string(kPrefix) + u"Re-define the item of the type '" + keys[i] + u"'");
    seen.push_back(parsed.type);
    JS::RootedObject entry(cx, JS_NewObjectWithGivenProto(cx, &kEntryClass, nullptr));
    JS::RootedObject pending(cx, JS::NewArrayObject(cx, 0));
    JS::RootedString type(cx, NewString(cx, parsed.type));
    JS::RootedValue promise(cx), entryValue(cx), typeValue(cx);
    if (!entry || !pending || !type || !JS_GetElement(cx, promises, i, &promise)) return false;
    JS::SetReservedSlot(entry, EntryType, JS::StringValue(type));
    JS::SetReservedSlot(entry, EntryStatus, JS::Int32Value(Loading));
    JS::SetReservedSlot(entry, EntryData, JS::UndefinedValue());
    JS::SetReservedSlot(entry, EntryPending, JS::ObjectValue(*pending));
    entryValue.setObject(*entry);
    typeValue.setString(type);
    JS::RootedObject source(cx, &promise.toObject());
    if (!JS_SetElement(cx, entries, i, entryValue) || !JS_SetElement(cx, types, i, typeValue) ||
        !LoadEntry(state, entry, source)) return false;
  }
  if (!JS_FreezeObject(cx, types)) return false;
  JS::SetReservedSlot(item, ItemStyle, JS::StringValue(styleText));
  JS::SetReservedSlot(item, ItemTypes, JS::ObjectValue(*types));
  JS::SetReservedSlot(item, ItemEntries, JS::ObjectValue(*entries));
  JS::SetReservedSlot(item, ItemOwner, JS::PrivateValue(&state));
  args.rval().setObject(*item);
  return trace.Finish(true);
}

bool StyleGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx, ItemReceiver(cx, args.thisv(), "get presentationStyle"));
  if (!self) return false;
  args.rval().set(JS::GetReservedSlot(self, ItemStyle));
  return JS_WrapValue(cx, args.rval());
}

// [Frozen, Cached] sequence<DOMString>: the same frozen array on every read.
bool TypesGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx, ItemReceiver(cx, args.thisv(), "get types"));
  if (!self) return false;
  args.rval().set(JS::GetReservedSlot(self, ItemTypes));
  return JS_WrapValue(cx, args.rval());
}

bool Supports(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!argc) return WebKit::Fail(cx, "ClipboardItem.supports: At least 1 argument required, but only 0 passed");
  std::u16string type;
  if (!ToUtf16(cx, args[0], &type)) return false;
  auto parsed = ParseClipboardType(type);
  args.rval().setBoolean(parsed.valid && parsed.supported);
  return true;
}
}  // namespace ruyijs::web_clipboard
