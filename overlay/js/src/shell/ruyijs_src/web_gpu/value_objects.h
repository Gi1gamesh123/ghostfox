#ifndef RUYIJS_WEB_GPU_VALUE_OBJECTS_H
#define RUYIJS_WEB_GPU_VALUE_OBJECTS_H
#include "web_gpu/state.h"

namespace ruyijs::web_gpu {
void RegisterState(State&);
void UnregisterState(State&);
State* StateForGlobal(JSObject* global);
State* ValueReceiver(JSContext*, JS::HandleValue, const JSClass*, JS::MutableHandleObject);
bool InstallValueInterfaces(State&);
bool InstallLimits(State&);
bool ConstructGpuError(JSContext*, unsigned, JS::Value*);
bool GpuErrorMessage(JSContext*, unsigned, JS::Value*);
bool LostInfoProperty(JSContext*, unsigned, JS::Value*);
bool AdapterInfoProperty(JSContext*, unsigned, JS::Value*);
bool LabelValueProperty(JSContext*, unsigned, JS::Value*);
bool CanvasProperty(JSContext*, unsigned, JS::Value*);
bool CompilationInfoProperty(JSContext*, unsigned, JS::Value*);
bool CompilationMessageProperty(JSContext*, unsigned, JS::Value*);
bool PipelineErrorProperty(JSContext*, unsigned, JS::Value*);
bool ValueMethod(JSContext*, unsigned, JS::Value*);
bool PipelineErrorConstructor(JSContext*, unsigned, JS::Value*);
bool LostReason(State&, JS::MutableHandleValue);
extern const JSClass kValidationErrorClass, kMemoryErrorClass, kInternalErrorClass;
extern const JSClass kLimitsClass, kLostInfoClass;
}
#endif
