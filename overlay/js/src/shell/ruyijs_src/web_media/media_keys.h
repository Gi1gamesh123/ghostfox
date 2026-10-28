#ifndef RUYIJS_WEB_MEDIA_MEDIA_KEYS_H
#define RUYIJS_WEB_MEDIA_MEDIA_KEYS_H
#include "web_media/state.h"

namespace ruyijs::web_media {
extern const JSClass kMediaKeySessionClass, kMediaKeyStatusMapClass,
    kMediaKeySystemAccessClass, kMediaKeysClass, kMediaKeyErrorClass;

bool InstallMediaKeys(State&);
bool InstallMediaKeyError(State&);
bool IsMediaKeysValue(JS::HandleValue);
}
#endif
