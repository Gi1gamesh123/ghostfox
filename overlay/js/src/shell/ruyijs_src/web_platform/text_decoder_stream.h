#pragma once
#include "web_platform/state.h"
#include "resource_network/native_transform.h"
#include "jsapi.h"
#include "js/Object.h"

namespace ruyijs::web_platform {
enum DecoderStreamSlot { DSEncoding,DSFatal,DSIgnoreBom,DSHandle,DSTransform,DSReadable,DSWritable,DSSlots };
extern const JSClass kTextDecoderStreamClass;
extern const resource_network::NativeTransformSteps kDecoderStreamSteps;
bool TextDecoderStreamConstructor(JSContext*,unsigned,JS::Value*);
bool InstallTextDecoderStream(State&,PageHelpers&);
bool DecoderStreamPrototype(JSContext*,const JS::CallArgs&,JS::MutableHandleObject);
}
