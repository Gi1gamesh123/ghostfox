#ifndef RUYIJS_WEB_MEDIA_LEGACY_VALUES_H
#define RUYIJS_WEB_MEDIA_LEGACY_VALUES_H
#include "web_media/state.h"
namespace ruyijs::web_media {
extern const JSClass kMediaDeviceInfoClass;
bool InstallLegacyValues(State&);
// The MediaDeviceInfo factory: the four IDL strings in declaration order
// (deviceId, kind, label, groupId) become the object's accessor slots.
bool NewMediaDeviceInfo(State&, const char* deviceId, const char* kind,
                        const char* label, const char* groupId,
                        JS::MutableHandleObject out);
}
#endif
