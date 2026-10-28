#ifndef RUYIJS_DOMAIN_WEB_MEDIA_CONFIGURATION_H
#define RUYIJS_DOMAIN_WEB_MEDIA_CONFIGURATION_H
#include "domain/web_media/codecs.h"
#include <cstdint>
namespace ruyijs::domain::web_media {
struct TrackConfiguration {
  bool present = false;
  std::string content_type;
  uint32_t width = 0, height = 0;
  double framerate = 0;
};
struct Configuration {
  TrackConfiguration audio, video;
  std::string type;
  bool encoding = false, encrypted = false;
};
inline const char* ValidateTrack(const TrackConfiguration& track, bool video) {
  if (!track.present) return nullptr;
  if (video && !(track.framerate > 0)) return "Invalid frame rate";
  if (video && (!track.width || !track.height)) return "Invalid VideoConfiguration!";
  const auto type = ParseMediaType(track.content_type);
  if (!type.valid || !type.mime.starts_with(video ? "video/" : "audio/"))
    return video ? "Invalid VideoConfiguration MIME type" : "Invalid AudioConfiguration MIME type";
  const bool container = type.mime == "video/mkv" || type.mime == "video/mp4" ||
      type.mime == "video/webm" || type.mime == "video/mpeg" || type.mime == "audio/ogg" ||
      type.mime == "audio/mp4" || type.mime == "audio/webm" || type.mime == "audio/mpeg";
  if (container && (!type.has_codecs || type.parameters != 1)) return "Container missing codec parameters";
  if (container && type.codecs.size() != 1) return "Container codecs must describe a single codec";
  return nullptr;
}
struct Capability { bool supported = false, smooth = false, power_efficient = false; };
inline Capability Evaluate(const Configuration& config) {
  if (config.encrypted || config.type == "webrtc") return {};
  // The reference record path advertises VP8 MIME support, but its platform
  // encoder capability check rejects video configurations.
  if (config.encoding && config.video.present) return {};
  bool supported = true;
  for (const auto* track : {&config.audio, &config.video}) {
    if (track->present) supported &= TypeSupported(ParseMediaType(track->content_type), config.encoding, config.type == "file");
  }
  // Match the reference software path's signed decoded image allocation bound.
  const uint64_t pixels = uint64_t(config.video.width) * config.video.height;
  if (config.video.present && (config.video.width > 32767 || config.video.height > 32767 || pixels > 0x7fffffff / 4)) supported = false;
  return {supported, supported, supported && (!config.video.present || pixels <= 640 * 480)};
}
}
#endif
