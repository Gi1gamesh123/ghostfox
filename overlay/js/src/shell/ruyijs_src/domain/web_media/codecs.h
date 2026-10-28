#ifndef RUYIJS_DOMAIN_WEB_MEDIA_CODECS_H
#define RUYIJS_DOMAIN_WEB_MEDIA_CODECS_H
#include "domain/web_media/mime.h"
#include <charconv>
namespace ruyijs::domain::web_media {
inline bool Number(std::string_view text, unsigned* value, int base = 10) {
  auto result = std::from_chars(text.data(), text.data() + text.size(), *value, base);
  return !text.empty() && result.ec == std::errc() && result.ptr == text.data() + text.size();
}
inline std::vector<std::string_view> Parts(std::string_view text) {
  std::vector<std::string_view> result;
  for (;;) {
    auto dot = text.find('.'); result.push_back(text.substr(0, dot));
    if (dot == text.npos) return result;
    text.remove_prefix(dot + 1);
  }
}
inline bool AVC(std::string_view codec) {
  if (codec.size() != 11 || (codec.substr(0, 5) != "avc1." && codec.substr(0, 5) != "avc3.")) return false;
  unsigned value;
  if (!Number(codec.substr(5), &value, 16)) return false;
  const unsigned profile = value >> 16, level = value & 255;
  return (profile == 66 || profile == 77 || profile == 88 || profile == 100 || profile == 110 || profile == 122 || profile == 244) && level >= 10 && level <= 62;
}
inline bool VP9(std::string_view codec) {
  if (codec == "vp9" || codec == "vp9.0") return true;
  auto parts = Parts(codec);
  if ((parts.size() != 4 && parts.size() != 9) || parts[0] != "vp09") return false;
  unsigned profile, level, depth;
  return Number(parts[1], &profile) && Number(parts[2], &level) && Number(parts[3], &depth) && profile <= 3 && level >= 10 && level <= 62 && (depth == 8 || depth == 10 || depth == 12);
}
inline bool AV1(std::string_view codec) {
  auto parts = Parts(codec);
  if ((parts.size() != 4 && parts.size() != 10) || parts[0] != "av01" || parts[2].size() != 3) return false;
  unsigned profile, level, depth;
  return Number(parts[1], &profile) && profile <= 2 && Number(parts[2].substr(0, 2), &level) && level <= 23 && (parts[2][2] == 'M' || parts[2][2] == 'H') && Number(parts[3], &depth) && (depth == 8 || depth == 10 || depth == 12);
}
inline bool HEVC(std::string_view codec) {
  auto parts = Parts(codec);
  if (parts.size() < 4 || parts.size() > 10 || (parts[0] != "hev1" && parts[0] != "hvc1")) return false;
  auto profile = parts[1];
  if (!profile.empty() && profile[0] >= 'A' && profile[0] <= 'C') profile.remove_prefix(1);
  unsigned value;
  if (!Number(profile, &value) || value > 31 || !Number(parts[2], &value, 16) || parts[3].size() < 2 || (parts[3][0] != 'L' && parts[3][0] != 'H') || !Number(parts[3].substr(1), &value) || value > 255) return false;
  for (size_t i = 4; i < parts.size(); ++i) if (!Number(parts[i], &value, 16) || value > 255) return false;
  return true;
}
inline bool CodecSupported(const MediaType& type, const std::string& codec, bool recorder) {
  const bool video = type.mime.starts_with("video/");
  if (type.mime.ends_with("/webm")) {
    if (codec == "opus") return true;
    if (codec == "vorbis") return !recorder;
    return video && (codec == "vp8" || codec == "vp8.0" || (!recorder && (VP9(codec) || AV1(codec))));
  }
  if (type.mime.ends_with("/mp4") && !recorder) {
    if (codec == "opus" || codec == "flac" || codec == "mp4a.40.2" || codec == "mp4a.40.5" || codec == "mp4a.40.29") return true;
    return video && (AVC(codec) || VP9(codec) || AV1(codec) || HEVC(codec));
  }
  if (type.mime == "audio/ogg") return recorder ? codec == "opus" : codec == "opus" || codec == "vorbis" || codec == "flac";
  return false;
}
inline bool TypeSupported(const MediaType& type, bool recorder, bool file = false) {
  if (!type.valid) return false;
  const bool container = type.mime == "audio/webm" || type.mime == "video/webm" || (!recorder && (type.mime == "audio/mp4" || type.mime == "video/mp4")) || ((file || recorder) && type.mime == "audio/ogg");
  if (!container) return file && (type.mime == "audio/flac" || type.mime == "audio/mpeg") && !type.has_codecs;
  if (recorder && type.has_codecs && type.codecs.empty()) return false;
  for (const auto& codec : type.codecs) if (!CodecSupported(type, codec, recorder)) return false;
  return true;
}
}
#endif
