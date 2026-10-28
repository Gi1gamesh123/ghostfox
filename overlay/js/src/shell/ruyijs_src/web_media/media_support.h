#ifndef RUYIJS_WEB_MEDIA_SUPPORT_H
#define RUYIJS_WEB_MEDIA_SUPPORT_H
#include "domain/web_media/codecs.h"
namespace ruyijs::web_media {
inline bool MediaTypeSupported(const std::string& value, bool recorder) {
  if (value.empty()) return recorder;
  return domain::web_media::TypeSupported(domain::web_media::ParseMediaType(value), recorder);
}
}
#endif
