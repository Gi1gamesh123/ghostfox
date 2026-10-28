#ifndef RUYIJS_TEXT_DECODER_INPUT_H
#define RUYIJS_TEXT_DECODER_INPUT_H
#include "js/RootingAPI.h"
#include <string>
namespace ruyijs::web_platform {
// WebIDL validates the BufferSource type before converting the options, then
// reads the bytes after conversion (a getter can detach or mutate the buffer).
bool DecoderBuffer(JSContext*, JS::HandleValue, JS::MutableHandleObject,bool streamCaller=false);
bool DecoderBytes(JSContext*, JS::HandleObject, std::string*);
bool DecoderOptions(JSContext*, JS::HandleValue, JS::MutableHandleObject,bool streamCaller=false);
}
#endif
