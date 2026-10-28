#ifndef RUYIJS_WEB_MEDIA_CONFIGURATION_H
#define RUYIJS_WEB_MEDIA_CONFIGURATION_H
#include "domain/web_media/configuration.h"
#include "runtime/web_kit.h"
namespace ruyijs::web_media {
bool ReadMediaConfiguration(JSContext*, JS::HandleValue, bool,
                            domain::web_media::Configuration*);
}
#endif
