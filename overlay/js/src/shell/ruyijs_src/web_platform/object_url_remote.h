#ifndef RUYIJS_WEB_PLATFORM_OBJECT_URL_REMOTE_H
#define RUYIJS_WEB_PLATFORM_OBJECT_URL_REMOTE_H
#include "js/RootingAPI.h"
#include <string>
namespace ruyijs::web_platform {
struct State;
bool RetainRemoteObjectUrl(State&, JS::HandleObject, const std::string& url);
bool RevokeRemoteObjectUrl(State&, const std::string& url);
// A Worker realm's read of a blob: URL held by the origin's store.
bool ReadRemoteObjectUrl(State&, const std::string& url, std::string* bytes, std::string* type);
}
#endif
