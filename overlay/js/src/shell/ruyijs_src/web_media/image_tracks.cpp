#include "web_media/image_decoder.h"
#include "js/Conversions.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
namespace ruyijs::web_media {
const JSClass kImageTrackClass = {"ImageTrack", JSCLASS_HAS_RESERVED_SLOTS(ITSlots)};
bool NewImageTracks(JSContext* cx, JS::HandleObject factories, JS::MutableHandleObject result) {
  JS::RootedObject prototype(cx), ready(cx, JS::NewPromiseObject(cx, nullptr));
  if (!ready || !ImageFactoryPrototype(cx, factories, IFTrackList, &prototype)) return false;
  JS::RootedObject expando(cx,JS_NewObjectWithGivenProto(cx,nullptr,nullptr));
  if(!expando)return false;
  JS::RootedValue privateValue(cx,JS::ObjectValue(*expando));
  js::ProxyOptions options;options.setClass(&kImageTrackListClass);
  result.set(js::NewProxyObject(cx,ImageTrackListHandler(),privateValue,prototype,options));
  if (!result) return false;
  js::SetProxyReservedSlot(result, ILReady, JS::ObjectValue(*ready));
  js::SetProxyReservedSlot(result, ILTrack, JS::NullValue()); return true;
}
bool EstablishImageTracks(JSContext* cx, JS::HandleObject decoder) {
  auto& image = ImageRecordOf(decoder)->image;
  JS::RootedObject factories(cx, ImageObject(decoder, IDFactories)), prototype(cx);
  if (!ImageFactoryPrototype(cx, factories, IFTrack, &prototype)) return false;
  JS::RootedObject track(cx, JS_NewObjectWithGivenProto(cx, &kImageTrackClass, prototype));
  JS::RootedObject list(cx, ImageObject(decoder, IDTracks)), ready(cx, ImageObject(list, ILReady));
  if (!track) return false;
  JS::SetReservedSlot(track, ITAnimated, JS::BooleanValue(image.animated));
  JS::SetReservedSlot(track, ITCount, JS::NumberValue(ImageRecordOf(decoder)->frameCount));
  JS::SetReservedSlot(track, ITRepeat, JS::NumberValue(image.repetitions));
  JS::SetReservedSlot(track, ITSelected, JS::BooleanValue(true));
  JS::SetReservedSlot(track, ITDecoder, JS::ObjectValue(*decoder));
  JS::RootedValue value(cx, JS::ObjectValue(*track));
  js::SetProxyReservedSlot(list, ILTrack, value);
  return JS::ResolvePromise(cx, ready, JS::UndefinedHandleValue);
}
bool ImageTrackGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const unsigned slot = unsigned(WebKit::Slot(args));
  static const char* names[] = {"get animated", "get frameCount", "get repetitionCount", "get selected"};
  JS::RootedObject self(cx);
  if (!ImageBrand(cx, args.thisv(), &kImageTrackClass, names[slot], &self)) return false;
  args.rval().set(JS::GetReservedSlot(self, slot)); return true;
}
bool ImageTrackSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); JS::RootedObject self(cx);
  if (!ImageBrand(cx, args.thisv(), &kImageTrackClass, "set selected", &self)) return false;
  if (!argc) return WebKit::Fail(cx, "ImageTrack.selected setter: At least 1 argument required, but only 0 passed");
  JSAutoRealm realm(cx,self);const bool selected=JS::ToBoolean(args[0]);
  if(selected!=JS::GetReservedSlot(self,ITSelected).toBoolean()) {
    JS::SetReservedSlot(self, ITSelected, JS::BooleanValue(selected));
    JS::RootedObject decoder(cx,ImageObject(self,ITDecoder));
    if(decoder&&!ResetImageDecoder(cx,decoder,"AbortError","Reset decoder (select index)"))return false;
  }
  args.rval().setUndefined(); return true;
}
bool ImageListGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); const unsigned field = unsigned(WebKit::Slot(args));
  static const char* names[] = {"get ready", "get length", "get selectedIndex", "get selectedTrack"};
  JS::RootedObject self(cx);
  if (!ImageBrand(cx, args.thisv(), &kImageTrackListClass, names[field], &self)) return false;
  JS::RootedObject track(cx, ImageObject(self, ILTrack));
  const bool selected = track && JS::GetReservedSlot(track, ITSelected).toBoolean();
  if (field == 0) args.rval().set(js::GetProxyReservedSlot(self, ILReady));
  if (field == 1) args.rval().setInt32(track ? 1 : 0);
  if (field == 2) args.rval().setInt32(selected ? 0 : -1);
  if (field == 3) args.rval().set(selected ? JS::ObjectValue(*track) : JS::NullValue());
  return JS_WrapValue(cx, args.rval());
}
}
