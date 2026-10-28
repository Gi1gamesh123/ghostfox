// HTMLMediaElement's text-track ports. DocumentModel keeps which list belongs
// to which element; this owner creates the TextTrackList and TextTrack objects
// with its own realm-local prototypes, so an iframe element gets that iframe's
// interfaces.
#include "web_media/realm.h"
#include "web_media/state.h"
#include "web_media/text_track.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::web_media {
bool NewElementTextTrackList(JSContext* cx, JS::MutableHandleObject list) {
  State* state = StateForGlobal(JS::CurrentGlobalOrNull(cx));
  if (!state) return WebKit::Fail(cx, "TextTrackList has no media realm");
  list.set(NewTextTrackList(*state));
  return list != nullptr;
}

// addTextTrack: a new track in the list's realm, mode "hidden" (HTML
// "addTextTrack" step 4), appended to the list and exposed at its index.
bool AppendElementTextTrack(JSContext* cx, JS::HandleObject list, const char* kind,
                            JS::HandleString label, JS::HandleString language,
                            JS::MutableHandleObject track) {
  JS::RootedObject target(cx, list);
  {
    JSAutoRealm realm(cx, target);
    State* state = StateForGlobal(JS::CurrentGlobalOrNull(cx));
    if (!state || JS::GetClass(target) != &kTextTrackListClass)
      return WebKit::Fail(cx, "TextTrackList has no media realm");
    JS::RootedString hidden(cx, JS_NewStringCopyZ(cx, "hidden"));
    JS::RootedValue labelValue(cx, JS::StringValue(label)),
        languageValue(cx, JS::StringValue(language));
    JS::RootedObject created(cx, NewTextTrack(*state, kind, "", ""));
    if (!hidden || !created || !JS_WrapValue(cx, &labelValue) ||
        !JS_WrapValue(cx, &languageValue)) return false;
    JS::SetReservedSlot(created, TrackLabel, labelValue);
    JS::SetReservedSlot(created, TrackLanguage, languageValue);
    JS::SetReservedSlot(created, TrackMode, JS::StringValue(hidden));
    JS::RootedValue source(cx, JS::GetReservedSlot(target, TrackListItems));
    JS::RootedObject items(cx, source.isObject() ? &source.toObject() : nullptr);
    uint32_t length = 0;
    if (!items || !JS::GetArrayLength(cx, items, &length)) return false;
    JS::RootedValue value(cx, JS::ObjectValue(*created));
    if (!JS_SetElement(cx, items, length, value) ||
        !JS_DefineElement(cx, target, length, value, JSPROP_ENUMERATE | JSPROP_READONLY))
      return false;
    track.set(created);
  }
  return true;
}
}  // namespace ruyijs::web_media
