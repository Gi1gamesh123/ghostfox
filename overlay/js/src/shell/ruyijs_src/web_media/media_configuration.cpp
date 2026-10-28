#include "web_media/media_configuration.h"
#include "jsapi.h"
#include "runtime/runtime.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include <cmath>
namespace ruyijs::web_media {
namespace {
using domain::web_media::TrackConfiguration;
bool Error(JSContext* cx, bool encoding, const std::string& message) {
  const std::string prefix = encoding ? "MediaCapabilities.encodingInfo: " : "MediaCapabilities.decodingInfo: ";
  return WebKit::Fail(cx, (prefix + message).c_str());
}
bool Dictionary(JSContext* cx, JS::HandleValue value, JS::MutableHandleObject out) {
  if (value.isNullOrUndefined()) { out.set(nullptr); return true; }
  if (!value.isObject()) return WebKit::Fail(cx, "Media configuration is not an object.");
  out.set(&value.toObject()); return true;
}
bool Read(JSContext* cx, JS::HandleObject object, const char* key, JS::MutableHandleValue value) {
  if (!object) { value.setUndefined(); return true; }
  return JS_GetProperty(cx, object, key, value);
}
bool String(JSContext* cx, JS::HandleValue value, std::string* result) {
  JS::RootedString text(cx, JS::ToString(cx, value));
  return text && ToUTF8(cx, text, result);
}
bool Track(JSContext* cx, JS::HandleValue value, bool video, bool encoding, TrackConfiguration* out) {
  if (value.isUndefined()) return true;
  out->present = true;
  JS::RootedObject object(cx);
  if (!Dictionary(cx, value, &object)) return false;
  const char* audioKeys[] = {"bitrate", "channels", "contentType", "samplerate"};
  const char* videoKeys[] = {"bitrate", "colorGamut", "contentType", "framerate", "hasAlphaChannel", "hdrMetadataType", "height", "scalabilityMode", "transferFunction", "width"};
  const char* dictionary = video ? "VideoConfiguration" : "AudioConfiguration";
  for (size_t i = 0; i < (video ? 10 : 4); ++i) {
    const std::string key = video ? videoKeys[i] : audioKeys[i];
    JS::RootedValue field(cx);
    if (!Read(cx, object, key.c_str(), &field)) return false;
    const bool required = key == "contentType" || (video && (key == "bitrate" || key == "framerate" || key == "height" || key == "width"));
    if (field.isUndefined()) {
      if (required) return Error(cx, encoding, "Missing required '" + key + "' member of " + dictionary + ".");
      continue;
    }
    if (key == "contentType") {
      if (!String(cx, field, &out->content_type)) return false;
    } else if (key == "framerate") {
      if (!JS::ToNumber(cx, field, &out->framerate)) return false;
      if (!std::isfinite(out->framerate)) return Error(cx, encoding, "'framerate' member of VideoConfiguration is not a finite floating-point value.");
    } else if (key == "width" || key == "height" || key == "samplerate") {
      uint32_t number;
      if (!JS::ToUint32(cx, field, &number)) return false;
      if (key == "width") out->width = number;
      if (key == "height") out->height = number;
    } else if (key == "bitrate") {
      double ignored;
      if (!JS::ToNumber(cx, field, &ignored)) return false;
    } else if (key != "hasAlphaChannel") {
      std::string text;
      if (!String(cx, field, &text)) return false;
      const bool bad = (key == "colorGamut" && text != "srgb" && text != "p3" && text != "rec2020") ||
          (key == "transferFunction" && text != "srgb" && text != "pq" && text != "hlg") ||
          (key == "hdrMetadataType" && text != "smpteSt2086" && text != "smpteSt2094-10" && text != "smpteSt2094-40");
      if (bad) return Error(cx, encoding, "Invalid enum value for " + key);
    }
  }
  return true;
}
}
bool ReadMediaConfiguration(JSContext* cx, JS::HandleValue value, bool encoding,
                            domain::web_media::Configuration* config) {
  config->encoding = encoding;
  JS::RootedObject object(cx);
  JS::RootedValue audio(cx), video(cx), keySystem(cx), type(cx);
  if (!Dictionary(cx, value, &object) || !Read(cx, object, "audio", &audio) ||
      !Track(cx, audio, false, encoding, &config->audio) || !Read(cx, object, "video", &video) ||
      !Track(cx, video, true, encoding, &config->video)) return false;
  if (!encoding) {
    if (!Read(cx, object, "keySystemConfiguration", &keySystem)) return false;
    config->encrypted = !keySystem.isUndefined();
  }
  if (!Read(cx, object, "type", &type)) return false;
  const std::string dictionary = encoding ? "MediaEncodingConfiguration" : "MediaDecodingConfiguration";
  if (type.isUndefined()) return Error(cx, encoding, "Missing required 'type' member of " + dictionary + ".");
  if (!String(cx, type, &config->type)) return false;
  const bool valid = config->type == "webrtc" || (encoding ? config->type == "record" : config->type == "file" || config->type == "media-source");
  if (!valid) return Error(cx, encoding, "'" + config->type + "' (value of 'type' member of " + dictionary + ") is not a valid value for enumeration " + (encoding ? "MediaEncodingType." : "MediaDecodingType."));
  return true;
}
}
