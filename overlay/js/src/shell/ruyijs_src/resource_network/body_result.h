#pragma once
#include "resource_network/body.h"

namespace ruyijs::resource_network {
// Value conversion only: the original public consumer owns the one Promise.
bool BodyTextValue(JSContext*,const BodyRecord*,JS::MutableHandleValue);
bool BodyJsonValue(JSContext*,const BodyRecord*,JS::MutableHandleValue,bool* rejected);
bool BodyArrayBufferValue(JSContext*,const BodyRecord*,JS::MutableHandleValue);
// bytes(): a Uint8Array over a fresh ArrayBuffer holding the body.
bool BodyBytesValue(JSContext*,const BodyRecord*,JS::MutableHandleValue);
bool BodyBlobValue(JSContext*,JS::HandleObject factories,const BodyRecord*,const std::string&,JS::MutableHandleValue);
bool BodyFormDataValue(JSContext*,JS::HandleObject factories,const BodyRecord*,const std::string&,JS::MutableHandleValue,const char**);
bool BodyBlobValue(State&,const BodyRecord*,const std::string& fullMime,JS::MutableHandleValue);
bool BodyFormDataValue(State&,const BodyRecord*,const std::string& fullMime,
                       JS::MutableHandleValue,const char** parseError);
}
