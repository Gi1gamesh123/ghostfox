#include "web_media/media_session.h"
#include "web_media/media_runtime.h"
#include "runtime/native_constructor_trace.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_media {
namespace {
enum MetadataSlot { MetadataTitle, MetadataArtist, MetadataAlbum, MetadataArtwork, MetadataSlots };
const JSClass kMetadataClass = {"MediaMetadata", JSCLASS_HAS_RESERVED_SLOTS(MetadataSlots)};

bool Receiver(JSContext* cx, JS::HandleValue value, JS::MutableHandleObject out) {
  return MediaReceiver(cx, value, &kMetadataClass, out);
}

bool Text(JSContext* cx, JS::HandleValue value, JS::MutableHandleString out) {
  out.set(JS::ToString(cx, value));
  return out;
}

bool MakeArtwork(JSContext* cx, JS::HandleValue value, JS::MutableHandleObject out) {
  if (!value.isObject()) return WebKit::Fail(cx, "MediaMetadata artwork must be an object");
  JS::RootedObject source(cx, &value.toObject());
  bool array = false;
  if (!JS::IsArrayObject(cx, source, &array) || !array)
    return WebKit::Fail(cx, "MediaMetadata artwork must be a sequence");
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, source, &length)) return false;
  JS::RootedObject result(cx, JS::NewArrayObject(cx, length));
  if (!result) return false;
  for (uint32_t i = 0; i < length; ++i) {
    JS::RootedValue item(cx);
    if (!JS_GetElement(cx, source, i, &item)) return false;
    if (!item.isObject()) return WebKit::Fail(cx, "MediaMetadata artwork item must be an object");
    if (!JS_SetElement(cx, result, i, item)) return false;
  }
  if (!JS_FreezeObject(cx, result)) return false;
  out.set(result);
  return true;
}

bool MetadataGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), &self)) return false;
  JS::RootedValue value(cx, JS::GetReservedSlot(self, WebKit::Slot(args)));
  if (!JS_WrapValue(cx, &value)) return false;
  args.rval().set(value);
  return true;
}

bool MetadataSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), &self) || !argc) return false;
  const int slot = WebKit::Slot(args);
  if (slot == MetadataArtwork) {
    JS::RootedObject artwork(cx);
    if (!MakeArtwork(cx, args[0], &artwork)) return false;
    JS::SetReservedSlot(self, slot, JS::ObjectValue(*artwork));
  } else {
    JS::RootedString text(cx);
    if (!Text(cx, args[0], &text)) return false;
    JS::SetReservedSlot(self, slot, JS::StringValue(text));
  }
  args.rval().setUndefined();
  return true;
}

bool MetadataConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing()) return trace.Finish(WebKit::Fail(cx, "MediaMetadata requires new"));
  auto& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject proto(cx);
  if (!state.kit.ConstructProto(args, state.metadata.prototype, &proto)) return false;
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kMetadataClass, proto));
  if (!object) return false;
  JS::RootedString empty(cx, JS_NewStringCopyZ(cx, ""));
  JS::RootedObject artwork(cx, JS::NewArrayObject(cx, 0));
  if (!empty || !artwork || !JS_FreezeObject(cx, artwork)) return false;
  JS::SetReservedSlot(object, MetadataTitle, JS::StringValue(empty));
  JS::SetReservedSlot(object, MetadataArtist, JS::StringValue(empty));
  JS::SetReservedSlot(object, MetadataAlbum, JS::StringValue(empty));
  JS::SetReservedSlot(object, MetadataArtwork, JS::ObjectValue(*artwork));
  if (argc && !args[0].isUndefined()) {
    if (!args[0].isObject()) return trace.Finish(WebKit::Fail(cx, "MediaMetadata init must be an object"));
    JS::RootedObject init(cx, &args[0].toObject());
    const char* names[] = {"title", "artist", "album"};
    const int slots[] = {MetadataTitle, MetadataArtist, MetadataAlbum};
    for (int i = 0; i < 3; ++i) {
      JS::RootedValue value(cx);
      if (!JS_GetProperty(cx, init, names[i], &value)) return false;
      if (!value.isUndefined()) {
        JS::RootedString text(cx);
        if (!Text(cx, value, &text)) return false;
        JS::SetReservedSlot(object, slots[i], JS::StringValue(text));
      }
    }
    JS::RootedValue value(cx);
    if (!JS_GetProperty(cx, init, "artwork", &value)) return false;
    if (!value.isUndefined()) {
      JS::RootedObject next(cx);
      if (!MakeArtwork(cx, value, &next)) return false;
      JS::SetReservedSlot(object, MetadataArtwork, JS::ObjectValue(*next));
    }
  }
  args.rval().setObject(*object);
  return trace.Finish(true);
}
}

bool IsMediaMetadataObject(JSObject* object) {
  return object && JS::GetClass(object) == &kMetadataClass;
}

bool InstallMediaMetadata(State& state) {
  JS::RootedObject parent(state.cx, JS::GetRealmObjectPrototype(state.cx));
  if (!state.kit.Define("MediaMetadata", &kMetadataClass, parent,
                        MetadataConstructor, &state.metadata)) return false;
  const char* fields[] = {"title", "artist", "album", "artwork"};
  for (int i = 0; i < 4; ++i)
    if (!state.kit.Accessor(state.metadata, "MediaMetadata", fields[i], MetadataGet,
                            MetadataSet, i)) return false;
  return MediaConstructorLast(state, state.metadata);
}

bool InstallMediaSession(State& state) {
  return InstallMediaMetadata(state) && InstallMediaSessionObject(state);
}
}
