#pragma once
#include "web_platform/state.h"
#include "resource_network/native_transform.h"
#include "jsapi.h"
#include "js/Object.h"

namespace ruyijs::web_platform {
enum CompressionSlot { CSTransform,CSReadable,CSWritable,CSSlots };
extern const JSClass kCompressionClass,kDecompressionClass,kCompressionOperationClass;
extern const resource_network::NativeTransformSteps kCompressionSteps;
bool CompressionConstructor(JSContext*,unsigned,JS::Value*);
bool DecompressionConstructor(JSContext*,unsigned,JS::Value*);
bool InstallCompressionStreams(State&,PageHelpers&);
bool CompressionPrototype(JSContext*,const JS::CallArgs&,bool,JS::MutableHandleObject);
}
