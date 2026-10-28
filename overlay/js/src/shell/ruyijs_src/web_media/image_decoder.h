#pragma once
#include "web_media/image_codec.h"
#include "web_media/state.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/Proxy.h"
#include <memory>

namespace ruyijs::web_media {
enum ImageFactory { IFDecoder, IFTrackList, IFTrack, IFFrame, IFColor, IFRect, IFException, IFFactories };
enum ImageSlot { IDRecord, IDType, IDComplete, IDCompleted, IDTracks, IDFactories,
                 IDHead, IDTail, IDReader, IDStreamFactories, IDSlots };
enum ImageListSlot { ILReady, ILTrack, ILSlots };
enum ImageTrackSlot { ITAnimated, ITCount, ITRepeat, ITSelected, ITDecoder, ITSlots };
enum FrameSlot { VFPixels, VFFormat, VFWidth, VFHeight, VFDisplayWidth, VFDisplayHeight,
                 VFTimestamp, VFDuration, VFFactories, VFColor, VFX, VFY, VFW, VFH, VFSlots };
enum DecodeRequestSlot { DRPromise, DRIndex, DRNext, DRSlots };
struct ImageRecord {
  std::string type;
  std::vector<uint8_t> bytes;
  domain::web_media::ImageData image;
  uint32_t width = 0, height = 0, frameCount = 0;
  size_t processedBytes = SIZE_MAX;
  bool processedEof = false, frameCountFinal = false;
  bool closed = false, unsupported = false, established = false;
  bool noColorConversion = false, preferAnimation = true;
  bool inputComplete = true, hasDesiredSize = false;
};
extern const JSClass kImageDecoderClass, kImageTrackListClass, kImageTrackClass, kVideoFrameClass;
inline JSObject* ImageObject(JSObject* object, unsigned slot) {
  const auto value = JS::GetClass(object)==&kImageTrackListClass ?
      js::GetProxyReservedSlot(object,slot) : JS::GetReservedSlot(object, slot);
  return value.isObject() ? &value.toObject() : nullptr;
}
inline ImageRecord* ImageRecordOf(JSObject* object) {
  const auto value = JS::GetReservedSlot(object, IDRecord);
  return value.isUndefined() ? nullptr : static_cast<ImageRecord*>(value.toPrivate());
}
bool ImageBrand(JSContext*, JS::HandleValue, const JSClass*, const char*, JS::MutableHandleObject);
bool ImageFactories(JSContext*, JS::HandleObject, JS::MutableHandleObject);
bool ImageFactoryPrototype(JSContext*, JS::HandleObject, unsigned, JS::MutableHandleObject);
bool ImageDesiredPrototype(JSContext*, const JS::CallArgs&, unsigned, JS::MutableHandleObject);
bool ImageException(JSContext*, JS::HandleObject factories, const char*, const char*);
bool ImageReject(JSContext*, JS::HandleObject promise);
bool ImageString(JSContext*, JS::HandleValue, std::string*);
bool ImageBuffer(JSContext*, JS::HandleValue, std::vector<uint8_t>*);
bool ImageAppendBytes(std::vector<uint8_t>*, const uint8_t*, size_t);
bool ImageUnsigned(JSContext*, JS::HandleValue, const char*, uint32_t*);
bool InstallImageDecoder(State&);
bool ImageDecoderConstructor(JSContext*,unsigned,JS::Value*);
bool ImageDecoderGet(JSContext*,unsigned,JS::Value*);
bool ImageDecoderMethod(JSContext*,unsigned,JS::Value*);
bool ImageTypeSupported(JSContext*,unsigned,JS::Value*);
bool ImageTrackGet(JSContext*,unsigned,JS::Value*);
bool ImageTrackSet(JSContext*,unsigned,JS::Value*);
bool ImageListGet(JSContext*,unsigned,JS::Value*);
bool NewImageTracks(JSContext*,JS::HandleObject,JS::MutableHandleObject);
const js::BaseProxyHandler* ImageTrackListHandler();
bool EstablishImageTracks(JSContext*,JS::HandleObject);
bool CloseImageDecoder(JSContext*,JS::HandleObject,const char*,const char*);
bool ResetImageDecoder(JSContext*,JS::HandleObject,const char*,const char*);
bool ScheduleImageMetadata(JSContext*,JS::HandleObject);
bool ProcessImageDecodes(JSContext*,JS::HandleObject);
bool StartImageStream(JSContext*,JS::HandleObject,JS::HandleObject);
bool CancelImageStream(JSContext*,JS::HandleObject);
bool VideoFrameConstructor(JSContext*,unsigned,JS::Value*);
bool VideoFrameGet(JSContext*,unsigned,JS::Value*);
bool VideoFrameMethod(JSContext*,unsigned,JS::Value*);
bool VideoFrameCopy(JSContext*,unsigned,JS::Value*);
bool NewVideoFrame(JSContext*,JS::HandleObject,const domain::web_media::ImageFrame&,JS::MutableHandleObject);
bool NewVideoColorSpace(JSContext*,JS::HandleObject,bool,JS::MutableHandleObject);
bool NewVideoColorSpaceFromInit(JSContext*,JS::HandleObject,JS::HandleValue,bool,JS::MutableHandleObject);
}
