#include "web_media/text_track.h"
#include "web_media/media_runtime.h"
#include "web_media/state.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"

namespace ruyijs::web_media {
namespace {
constexpr int ListLengthOp = 90;
bool List(JSContext* cx, JS::HandleValue value, const JSClass* clasp,
          JS::MutableHandleObject out) {
  out.set(value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  return (out && JS::GetClass(out) == clasp) || WebKit::Fail(cx, "Illegal track list receiver");
}
bool Items(JSContext* cx, JS::HandleObject list, unsigned slot,
           JS::MutableHandleObject out) {
  JS::RootedValue value(cx, JS::GetReservedSlot(list, slot));
  out.set(value.isObject() ? &value.toObject() : nullptr);
  return out != nullptr || WebKit::Fail(cx, "Track list has no items");
}
bool Length(JSContext* cx, JS::HandleObject items, JS::MutableHandleValue out) {
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, items, &length)) return false;
  out.setInt32(static_cast<int32_t>(length));
  return true;
}
bool Element(JSContext* cx, JS::HandleObject items, uint32_t index,
             JS::MutableHandleValue out) {
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, items, &length)) return false;
  if (index >= length) { out.setNull(); return true; }
  return JS_GetElement(cx, items, index, out) && JS_WrapValue(cx, out);
}
bool IdEquals(JSContext* cx, JS::HandleValue value, JS::HandleString id, bool* equal) {
  if (!value.isObject()) { *equal = false; return true; }
  JS::RootedObject object(cx, &value.toObject());
  JS::RootedValue candidate(cx);
  if (!JS_GetProperty(cx, object, "id", &candidate)) return false;
  JS::RootedString text(cx, JS::ToString(cx, candidate));
  int32_t comparison = 1;
  if (!text || !JS_CompareStrings(cx, text, id, &comparison)) return false;
  *equal = comparison == 0;
  return true;
}
}

bool CueListMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!List(cx, args.thisv(), &kTextTrackCueListClass, &self)) return false;
  JS::RootedObject items(cx);
  if (!Items(cx, self, CueListItems, &items)) return false;
  const int op = WebKit::Slot(args);
  if (op == ListLengthOp) return Length(cx, items, args.rval());
  uint32_t index = 0;
  if (op == 100) {
    if (!argc || !JS::ToUint32(cx, args[0], &index)) return false;
    return Element(cx, items, index, args.rval());
  }
  JS::RootedString id(cx, argc ? JS::ToString(cx, args[0]) : nullptr);
  if (!id) return false;
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, items, &length)) return false;
  for (uint32_t i = 0; i < length; ++i) {
    JS::RootedValue value(cx);
    if (!JS_GetElement(cx, items, i, &value)) return false;
    bool equal = false;
    if (IdEquals(cx, value, id, &equal) && equal) {
      args.rval().set(value);
      return JS_WrapValue(cx, args.rval());
    }
  }
  args.rval().setNull();
  return true;
}

bool TrackListMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!List(cx, args.thisv(), &kTextTrackListClass, &self)) return false;
  JS::RootedObject items(cx);
  if (!Items(cx, self, TrackListItems, &items)) return false;
  const int op = WebKit::Slot(args);
  if (op == ListLengthOp) return Length(cx, items, args.rval());
  // onchange/onaddtrack/onremovetrack read the handler TrackListSet stored.
  if (op >= TrackListChange && op <= TrackListRemove) {
    args.rval().set(JS::GetReservedSlot(self, op));
    return JS_WrapValue(cx, args.rval());
  }
  if (!argc)
    return WebKit::Fail(cx, "TextTrackList.getTrackById: At least 1 argument required, but only 0 passed");
  JS::RootedString id(cx, JS::ToString(cx, args[0]));
  if (!id) return false;
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, items, &length)) return false;
  for (uint32_t i = 0; i < length; ++i) {
    JS::RootedValue value(cx);
    if (!JS_GetElement(cx, items, i, &value)) return false;
    bool equal = false;
    if (IdEquals(cx, value, id, &equal) && equal) {
      args.rval().set(value);
      return JS_WrapValue(cx, args.rval());
    }
  }
  args.rval().setNull();
  return true;
}

bool TrackListSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!List(cx, args.thisv(), &kTextTrackListClass, &self)) return false;
  if (!argc) return WebKit::Fail(cx, "TextTrackList setter requires an argument");
  auto& state = *static_cast<State*>(JS::GetReservedSlot(self, TrackListOwner).toPrivate());
  const int slot = WebKit::Slot(args);
  const char* names[] = {"change", "addtrack", "removetrack"};
  if (!MediaHandlerSet(state, self, slot, names[slot - TrackListChange], args[0])) return false;
  args.rval().setUndefined();
  return true;
}

bool TextCueListIterator(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!List(cx, args.thisv(), &kTextTrackCueListClass, &self)) return false;
  auto& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedValue source(cx, JS::GetReservedSlot(self, CueListItems));
  JS::RootedObject items(cx, source.isObject() ? &source.toObject() : nullptr);
  if (!items) return WebKit::Fail(cx, "Track list has no iterator source");
  return JS::Call(cx, items, state.arrayValues, JS::HandleValueArray::empty(), args.rval());
}

bool TextTrackListIterator(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!List(cx, args.thisv(), &kTextTrackListClass, &self)) return false;
  auto& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedValue source(cx, JS::GetReservedSlot(self, TrackListItems));
  JS::RootedObject items(cx, source.isObject() ? &source.toObject() : nullptr);
  if (!items) return WebKit::Fail(cx, "Track list has no iterator source");
  return JS::Call(cx, items, state.arrayValues, JS::HandleValueArray::empty(), args.rval());
}
}
