#ifndef RUYIJS_DOMAIN_WEB_MEDIA_CAN_PLAY_H
#define RUYIJS_DOMAIN_WEB_MEDIA_CAN_PLAY_H
// HTMLMediaElement.canPlayType: Firefox DecoderTraits::CanHandleContainerType
// over the desktop decoders (Ogg, Wave, WebM, MP4, MP3, ADTS, FLAC, Matroska)
// with media.{av1,hevc,mkv}.enabled on. Pure rules; no engine dependency.
#include "domain/web_media/codecs.h"
namespace ruyijs::domain::web_media {
enum class CanPlay { No, Maybe, Probably };
inline bool AnyOf(const std::string& value, std::initializer_list<const char*> names) {
  for (const char* name : names) if (value == name) return true;
  return false;
}
inline bool AACCodec(const std::string& c) {
  return AnyOf(c, {"mp4a.40.2", "mp4a.40.02", "mp4a.40.5", "mp4a.40.05", "mp4a.67", "mp4a.40.29"});
}
inline bool VP8Codec(const std::string& c) {
  if (c == "vp8" || c == "vp8.0") return true;
  auto parts = Parts(c);
  unsigned value;
  return (parts.size() == 4 || parts.size() == 9) && parts[0] == "vp08" && Number(parts[1], &value) &&
         Number(parts[2], &value) && Number(parts[3], &value);
}
enum class Container { None, Ogg, Wave, WebM, MP4, MP3, ADTS, Flac, Matroska };
inline Container ContainerOf(const std::string& m) {
  if (AnyOf(m, {"audio/ogg", "application/ogg"})) return Container::Ogg;
  if (AnyOf(m, {"audio/wave", "audio/x-wav", "audio/wav", "audio/x-pn-wav"})) return Container::Wave;
  if (AnyOf(m, {"audio/webm", "video/webm"})) return Container::WebM;
  if (AnyOf(m, {"audio/mp4", "audio/x-m4a", "video/mp4", "video/quicktime", "video/x-m4v"})) return Container::MP4;
  if (AnyOf(m, {"audio/mp3", "audio/mpeg"})) return Container::MP3;
  if (AnyOf(m, {"audio/aac", "audio/aacp", "audio/x-aac"})) return Container::ADTS;
  if (AnyOf(m, {"audio/flac", "audio/x-flac", "application/x-flac"})) return Container::Flac;
  if (AnyOf(m, {"video/matroska", "video/x-matroska", "audio/matroska", "audio/x-matroska"})) return Container::Matroska;
  return Container::None;
}
// One codec of a typed container; |video| is the container's video-ness.
inline bool ContainerCodec(Container kind, const std::string& c, bool video) {
  switch (kind) {
    case Container::Ogg: return c == "opus" || c == "vorbis" || c == "flac";
    case Container::WebM:
      return c == "opus" || c == "vorbis" || (video && (VP9(c) || VP8Codec(c))) || (video && AV1(c));
    case Container::MP4:
      return AACCodec(c) || c == "mp3" || AnyOf(c, {"opus", "Opus", "flac", "fLaC"}) ||
             (video && (VP9(c) || AV1(c) || HEVC(c) || AVC(c)));
    case Container::Matroska:
      return AACCodec(c) || c == "opus" || c == "vorbis" ||
             (video && (AVC(c) || HEVC(c) || VP9(c) || VP8Codec(c) || AV1(c)));
    default: return false;
  }
}
// Firefox compares the whole codecs parameter for the single-codec containers.
inline bool SingleCodec(Container kind, const MediaType& type) {
  if (type.codecs.empty()) return true;
  if (type.codecs.size() != 1) return false;
  const std::string& c = type.codecs[0];
  if (kind == Container::Wave) return AnyOf(c, {"1", "3", "6", "7"});
  if (kind == Container::MP3) return c == "mp3";
  if (kind == Container::ADTS) return c == "aac";
  return false;
}
inline CanPlay CanPlayType(std::string_view raw) {
  const MediaType type = ParseMediaType(raw);
  if (!type.valid) return CanPlay::No;
  const Container kind = ContainerOf(type.mime);
  if (kind == Container::None) return CanPlay::No;
  if (!type.has_codecs) return CanPlay::Maybe;
  if (kind == Container::Flac) return CanPlay::Probably;
  if (kind == Container::Wave || kind == Container::MP3 || kind == Container::ADTS)
    return SingleCodec(kind, type) ? CanPlay::Probably : CanPlay::No;
  // An empty codecs list: Ogg accepts it outright; the track-based decoders
  // fall through to the container-only answer.
  if (type.codecs.empty()) return kind == Container::Ogg ? CanPlay::Probably : CanPlay::Maybe;
  const bool video = type.mime.starts_with("video/");
  for (const auto& codec : type.codecs)
    if (!ContainerCodec(kind, codec, video)) return CanPlay::No;
  return CanPlay::Probably;
}
inline const char* CanPlayText(CanPlay value) {
  return value == CanPlay::Probably ? "probably" : value == CanPlay::Maybe ? "maybe" : "";
}
}
#endif
