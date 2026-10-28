#pragma once
#include <string>
#include "js/RootingAPI.h"

namespace ruyijs::web_platform {
struct State;
// Original realm factories retained independently of mutable public properties.
bool InstallBodyFactories(State&);
bool BodyFactories(JSContext*,JS::HandleObject global,JS::MutableHandleObject);
bool BodyBlob(JSContext*,JS::HandleObject factories,const std::string& bytes,
               const std::string& type,JS::MutableHandleObject);
bool BodyFile(JSContext*,JS::HandleObject factories,const std::string& bytes,
               const std::string& type,const std::string& name,JS::MutableHandleObject);
bool BodyFormData(JSContext*,JS::HandleObject factories,JS::MutableHandleObject);
// Inputs are already normalized by the original consumer, not public WebIDL args.
bool BodyFormAppend(JSContext*,JS::HandleObject form,JS::HandleValue name,
                     JS::HandleValue value);
bool BodyFormUrlencoded(JSContext*,JS::HandleObject form,const std::string& bytes);
}
