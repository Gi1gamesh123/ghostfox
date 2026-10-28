#ifndef RUYIJS_WEB_AUDIO_NODE_FIELDS_H
#define RUYIJS_WEB_AUDIO_NODE_FIELDS_H
#include "web_audio/internal.h"

namespace ruyijs::web_audio {
enum class FieldType { Number, Boolean, String, Param, Buffer, Curve, Event, Null, Map, Bins, Port };
struct NodeField {
  int32_t node;
  const char* name;
  FieldType type;
  bool writable = false;
  double initial = 0;
  double minimum = -3.4028234663852886e38;
  double maximum = 3.4028234663852886e38;
  const char* text = nullptr;
};
using FT = FieldType;
inline constexpr NodeField kNodeFields[] = {
 {1,"fftSize",FT::Number,true,2048,32,32768},
 {1,"minDecibels",FT::Number,true,-100},
 {1,"maxDecibels",FT::Number,true,-30},
 {1,"smoothingTimeConstant",FT::Number,true,0.8,0,1},
 {1,"frequencyBinCount",FT::Bins},
 {2,"type",FT::String,true,0,0,0,"lowpass"},
 {2,"frequency",FT::Param,false,350,0,22050},
 {2,"detune",FT::Param,false,0,-153600,153600},
 {2,"Q",FT::Param,false,1},
 {2,"gain",FT::Param,false,0},
 {5,"buffer",FT::Buffer,true}, {5,"normalize",FT::Boolean,true,1},
 {6,"delayTime",FT::Param,false,0,0,1},
 {7,"reduction",FT::Number,false,0},
 {9,"panningModel",FT::String,true,0,0,0,"equalpower"},
 {9,"distanceModel",FT::String,true,0,0,0,"inverse"},
 {9,"positionX",FT::Param}, {9,"positionY",FT::Param}, {9,"positionZ",FT::Param},
 {9,"orientationX",FT::Param,false,1},
 {9,"orientationY",FT::Param}, {9,"orientationZ",FT::Param},
 {9,"refDistance",FT::Number,true,1,0},
 {9,"maxDistance",FT::Number,true,10000,0},
 {9,"rolloffFactor",FT::Number,true,1,0},
 {9,"coneInnerAngle",FT::Number,true,360},
 {9,"coneOuterAngle",FT::Number,true,360},
 {9,"coneOuterGain",FT::Number,true,0,0,1},
 {10,"bufferSize",FT::Number,false,0}, {10,"onaudioprocess",FT::Event,true},
 {11,"pan",FT::Param,false,0,-1,1},
 {12,"curve",FT::Curve,true}, {12,"oversample",FT::String,true,0,0,0,"none"},
 {13,"offset",FT::Param,false,1},
 {14,"buffer",FT::Buffer,true}, {14,"playbackRate",FT::Param,false,1},
 {14,"detune",FT::Param,false,0,-153600,153600},
 {14,"loop",FT::Boolean,true}, {14,"loopStart",FT::Number,true},
 {14,"loopEnd",FT::Number,true},
 {15,"onprocessorerror",FT::Event,true}, {15,"parameters",FT::Map}, {15,"port",FT::Port},
 {16,"mediaElement",FT::Null}, {17,"mediaStream",FT::Null}, {18,"stream",FT::Null}
};
inline constexpr size_t kNodeFieldCount = sizeof(kNodeFields)/sizeof(kNodeFields[0]);
bool InitNodeFields(State&, JS::HandleObject, JS::HandleValue options);
bool FieldReceiver(JSContext*, const JS::CallArgs&, int32_t, JS::MutableHandleObject);
bool ReadField(JSContext*, JS::HandleObject, const char*, JS::MutableHandleValue);
bool WriteField(State&, JS::HandleObject, int32_t, JS::HandleValue);
bool NodeFieldGet(JSContext*, unsigned, JS::Value*);
bool NodeFieldSet(JSContext*, unsigned, JS::Value*);
bool NodeExtraMethod(JSContext*, unsigned, JS::Value*);
bool InstallNodeSurface(State&);
bool InstallContextSurface(State&);
bool InstallAuxiliary(State&, JS::HandleObject);
bool InitListener(State&, JS::HandleObject);
bool WorkletPort(JSContext*, unsigned, JS::Value*);
bool AudioEventGet(JSContext*, unsigned, JS::Value*);
bool AudioEventSet(JSContext*, unsigned, JS::Value*);
bool ReplaceHandler(State&, JS::HandleObject, const char*, JS::HandleValue,
                    JS::MutableHandleValue);
JSObject* NewParamMap(State&);
bool DecodeAudioData(JSContext*, unsigned, JS::Value*);
bool MediaFactory(JSContext*, unsigned, JS::Value*);
inline constexpr int32_t kNodeFieldsSlot = 13;
inline constexpr int32_t kNodeEndedSlot = 14;
inline constexpr int32_t kContextStateHandlerSlot = 9;
extern const JSClass kParamMapClass;
}
#endif
