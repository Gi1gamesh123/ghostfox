// Document members of presentation features this runtime has no surface for:
// fullscreen, picture-in-picture, pointer lock, mouse capture, style sheet sets,
// storage access and ARIA notifications (Firefox 158 Document.webidl and its
// partials). Nothing is ever fullscreen, in picture-in-picture or pointer-locked,
// so the state members report Firefox's values for such a document; the
// operations follow Firefox's argument rules and outcomes (D149).
#include "document_model/state.h"
#include "document_model/exception_internal.h"
#include "browser_environment/environment.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/WeakMap.h"
#include "js/Wrapper.h"
#include <iterator>
#include <string>

namespace ruyijs::document_model {
namespace {
enum Member {
  // Accessors.
  kFullscreen, kMozFullScreen, kFullscreenEnabled, kMozFullScreenEnabled, kPictureInPictureEnabled, kSelectedSet, kLastSet,
  kPreferredSet, kSheetSets, kActiveViewTransition, kPointerLockElement, kFullscreenElement,
  kMozFullScreenElement, kPictureInPictureElement, kAccessorCount,
  // Methods.
  kReleaseCapture = kAccessorCount, kMozSetImageElement, kCaptureEvents, kReleaseEvents, kExitFullscreen,
  kMozCancelFullScreen, kExitPictureInPicture, kExitPointerLock, kEnableSheetsForSet, kHasStorageAccess,
  kRequestStorageAccess, kAriaNotify, kMemberCount
};
const char* const kNames[] = {
  "fullscreen", "mozFullScreen", "fullscreenEnabled", "mozFullScreenEnabled", "pictureInPictureEnabled", "selectedStyleSheetSet",
  "lastStyleSheetSet", "preferredStyleSheetSet", "styleSheetSets", "activeViewTransition", "pointerLockElement",
  "fullscreenElement", "mozFullScreenElement", "pictureInPictureElement",
  "releaseCapture", "mozSetImageElement", "captureEvents", "releaseEvents", "exitFullscreen", "mozCancelFullScreen",
  "exitPictureInPicture", "exitPointerLock", "enableStyleSheetsForSet", "hasStorageAccess", "requestStorageAccess",
  "ariaNotify"};
static_assert(std::size(kNames) == kMemberCount);
const unsigned kRequired[kMemberCount - kAccessorCount] = {0, 2, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1};
// Operations whose WebIDL return type is a Promise report failures as rejections.
bool Promised(int member) {
  return member == kExitFullscreen || member == kMozCancelFullScreen || member == kExitPictureInPicture || member == kHasStorageAccess ||
         member == kRequestStorageAccess;
}

bool Fail(JSContext* cx, const std::string& text) { return TypeError(cx, text.c_str()); }
// The per-document record of this owner: {last: lastStyleSheetSet, sets: DOMStringList}.
bool Record(State& state, JS::HandleObject document, JS::MutableHandleObject record) {
  JSContext* cx = state.cx;
  if (!state.presentation) state.presentation = JS::NewWeakMapObject(cx);
  JS::RootedObject map(cx, state.presentation);
  JS::RootedValue key(cx, JS::ObjectValue(*document)), found(cx);
  if (!map || !JS::GetWeakMapEntry(cx, map, key, &found)) return false;
  if (found.isObject()) { record.set(&found.toObject()); return true; }
  JS::RootedObject values(cx, JS::NewArrayObject(cx, 0));
  JS::RootedObject sets(cx, values ? state.environment.StringList(values) : nullptr);
  record.set(sets ? JS_NewPlainObject(cx) : nullptr);
  JS::RootedValue value(cx, JS::ObjectOrNullValue(sets));
  return record && JS_DefineProperty(cx, record, "sets", value, 0) &&
         JS_DefineProperty(cx, record, "last", JS::NullHandleValue, JSPROP_ENUMERATE) &&
         JS::SetWeakMapEntry(cx, map, key, JS::RootedValue(cx, JS::ObjectValue(*record)));
}
// The Document behind the receiver, or Firefox's brand error for |label|.
bool Receiver(JSContext* cx, const JS::CallArgs& args, const std::string& label, JS::MutableHandleObject document) {
  document.set(args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (document && JS::GetClass(document) == &kNodeClass && DocumentReceiver(cx, document)) return true;
  if (JS_IsExceptionPending(cx)) JS_ClearPendingException(cx);
  return Fail(cx, "'" + label + "' called on an object that does not implement interface Document.");
}
bool Operate(JSContext* cx, const JS::CallArgs& args, State& state, int member) {
  const std::string name = kNames[member];
  JS::RootedObject document(cx);
  if (!Receiver(cx, args, (member < kAccessorCount ? "get " : "") + name, &document)) return false;
  state.Observe("Document", kNames[member], int(member < kAccessorCount ? ApiAccess::Read : ApiAccess::Call));
  args.rval().setUndefined();
  if (member < kAccessorCount) {
    JS::RootedObject record(cx);
    switch (member) {
      case kFullscreen: case kMozFullScreen: args.rval().setBoolean(false); return true;
      // fullscreenEnabled: true unless a permissions policy disables "fullscreen";
      // no policy owner exists, so a permitted document's value.
      case kFullscreenEnabled: case kMozFullScreenEnabled: case kPictureInPictureEnabled:
        args.rval().setBoolean(true); return true;
      case kSelectedSet: case kPreferredSet: args.rval().setString(JS_GetEmptyString(cx)); return true;
      case kLastSet: return Record(state, document, &record) && JS_GetProperty(cx, record, "last", args.rval());
      case kSheetSets: return Record(state, document, &record) && JS_GetProperty(cx, record, "sets", args.rval());
      default: args.rval().setNull(); return true;  // the five element/transition members
    }
  }
  const unsigned required = kRequired[member - kAccessorCount];
  if (args.length() < required)
    return Fail(cx, "Document." + name + ": At least " + std::to_string(required) + " argument" +
                    (required == 1 ? "" : "s") + " required, but only " + std::to_string(args.length()) + " passed");
  if (member == kMozSetImageElement) {
    if (!JS::ToString(cx, args[0])) return false;
    bool element = false;
    if (!args[1].isNull() && (!args[1].isObject() || !state.sourceRealm->IsElementValue(args[1], &element) || !element))
      return !JS_IsExceptionPending(cx) && Fail(cx, "Document.mozSetImageElement: Argument 2 " +
          std::string(args[1].isObject() ? "does not implement interface Element." : "is not an object."));
    return true;
  }
  if (member == kEnableSheetsForSet || member == kAriaNotify) return JS::ToString(cx, args[0]) != nullptr;
  if (member == kExitFullscreen || member == kMozCancelFullScreen)
    return Fail(cx, "Not in fullscreen mode");
  if (member == kExitPictureInPicture)
    return ThrowDOM(state, "InvalidStateError", "No element is currently in picture-in-picture");
  if (member == kHasStorageAccess) { args.rval().setBoolean(true); return true; }
  return true;  // releaseCapture, captureEvents, releaseEvents, exitPointerLock, requestStorageAccess
}
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  const int member = FunctionSlot(args);
  const bool ok = Operate(cx, args, state, member);
  if (!Promised(member)) return ok;
  JS::RootedValue value(cx, args.rval());
  JSObject* promise = nullptr;
  if (ok) {
    promise = JS::CallOriginalPromiseResolve(cx, member == kRequestStorageAccess ? JS::UndefinedHandleValue : value);
  } else {
    JS::RootedValue error(cx);
    if (!JS_GetPendingException(cx, &error)) return false;
    JS_ClearPendingException(cx);
    promise = JS::CallOriginalPromiseReject(cx, error);
  }
  if (!promise) return false;
  args.rval().setObject(*promise);
  return true;
}
// selectedStyleSheetSet's setter records the name as lastStyleSheetSet; with no
// titled style sheet sets the selection itself stays "".
bool SetSelected(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  JS::RootedObject document(cx);
  if (!Receiver(cx, args, "set selectedStyleSheetSet", &document)) return false;
  state.Observe("Document", "selectedStyleSheetSet", int(ApiAccess::Write));
  JS::RootedString text(cx, JS::ToString(cx, args.get(0)));
  JS::RootedObject record(cx);
  if (!text || !Record(state, document, &record)) return false;
  JS::RootedValue value(cx, JS::StringValue(text));
  args.rval().setUndefined();
  return JS_SetProperty(cx, record, "last", value);
}
// fullscreen, fullscreenEnabled and fullscreenElement are [LegacyLenientSetter]:
// a brand-checked no-op.
bool Lenient(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject document(cx);
  args.rval().setUndefined();
  return Receiver(cx, args, std::string("set ") + kNames[FunctionSlot(args)], &document);
}
}  // namespace

bool InstallDocumentPresentation(State& state) {
  for (int member = 0; member < kAccessorCount; ++member)
    if (!state.Accessor("Document", kNames[member], Call, member,
                        member == kSelectedSet ? SetSelected :
                        member == kFullscreen || member == kFullscreenEnabled || member == kFullscreenElement
                            ? Lenient : nullptr))
      return false;
  for (int member = kAccessorCount; member < kMemberCount; ++member) {
    const unsigned length = member == kMozSetImageElement ? 2 : kRequired[member - kAccessorCount];
    if (!state.Method("Document", kNames[member], Call, length, member)) return false;
  }
  return true;
}
}  // namespace ruyijs::document_model
