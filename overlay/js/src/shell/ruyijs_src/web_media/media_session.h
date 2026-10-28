#ifndef RUYIJS_WEB_MEDIA_SESSION_H
#define RUYIJS_WEB_MEDIA_SESSION_H
#include "web_media/state.h"
namespace ruyijs::web_media {
bool IsMediaMetadataObject(JSObject*);
bool InstallMediaMetadata(State&);
bool InstallMediaSessionObject(State&);
bool InstallMediaSession(State&);
}
#endif
