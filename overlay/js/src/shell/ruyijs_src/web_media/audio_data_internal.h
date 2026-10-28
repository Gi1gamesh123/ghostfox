#ifndef RUYIJS_WEB_MEDIA_AUDIO_DATA_INTERNAL_H
#define RUYIJS_WEB_MEDIA_AUDIO_DATA_INTERNAL_H
// Shared by audio_data.cpp (construction, getters) and audio_data_copy.cpp
// (allocationSize/copyTo/clone/close) of the web_media AudioData surface.
#include "web_media/audio_data.h"
#include "domain/web_media/audio_samples.h"
#include "js/CallArgs.h"
#include "js/RootingAPI.h"
#include <string>
namespace ruyijs::web_media {
enum AudioSlot { AdFormat, AdRate, AdFrames, AdChannels, AdTimestamp, AdBytes, AdSlotCount };
namespace audio_detail {
inline constexpr const char* kMethods[] = {"allocationSize", "copyTo", "clone", "close"};
bool Fail(JSContext* cx, const std::string& message);
bool Receiver(JSContext* cx, const JS::CallArgs& args, const std::string& member, JS::MutableHandleObject out);
bool Closed(JSObject* self);
bool Member(JSContext* cx, JS::HandleObject dict, const char* name, JS::MutableHandleValue value, bool* present);
bool Required(JSContext* cx, const std::string& where, const char* dict, const char* name, bool present);
bool EnforceRange(JSContext* cx, JS::HandleValue value, double low, double high, const std::string& what, double* out);
bool Format(JSContext* cx, JS::HandleValue value, const std::string& where, const char* dict,
            domain::web_media::SampleFormat* out);
JSObject* Make(State& state, JS::HandleObject proto, double format, double rate, double frames,
               double channels, double timestamp, JS::HandleObject bytes);
bool Method(JSContext* cx, unsigned argc, JS::Value* vp);
}
}
#endif
