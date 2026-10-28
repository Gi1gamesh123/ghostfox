#ifndef RUYIJS_WEB_MEDIA_CONSTRAINTS_H
#define RUYIJS_WEB_MEDIA_CONSTRAINTS_H
#include "domain/web_media/constraints.h"
#include "js/RootingAPI.h"
namespace ruyijs::web_media {
bool ConstraintDictionary(JSContext*, JS::HandleValue, JS::MutableHandleObject);
bool ConstraintRead(JSContext*, JS::HandleObject, const char*, JS::MutableHandleValue);
bool ConvertConstraint(JSContext*, JS::HandleValue, domain::web_media::ConstraintKind);
bool ReadDisplayConstraints(JSContext*, JS::HandleValue);
bool ReadOutputOptions(JSContext*, JS::HandleValue);
bool MediaDeviceRequest(JSContext*, unsigned, JS::Value*);
bool MediaSupportedConstraints(JSContext*, unsigned, JS::Value*);
}
#endif
