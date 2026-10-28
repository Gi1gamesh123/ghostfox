#pragma once
#include "interaction/internal.h"
#include "js/Proxy.h"

namespace ruyijs::interaction {
enum TouchSlot : uint32_t {
  kTouchIdentifier, kTouchTarget, kTouchScreenX, kTouchScreenY,
  kTouchClientX, kTouchClientY, kTouchPageX, kTouchPageY,
  kTouchRadiusX, kTouchRadiusY, kTouchRotation, kTouchForce,
  kTouchOriginalTarget, kTouchSlotCount
};
extern const JSClass kTouchClass, kTouchListClass;
constexpr uint32_t kTouchWidgetMembers = kEventPublicFirst;
extern const JSPropertySpec kTouchValueProperties[], kTouchListProperties[];
extern const JSFunctionSpec kTouchListMethods[], kTouchEventMethods[];
bool IsTouch(JSObject*);
bool IsTouchList(JSObject*);
JSObject* TouchReceiver(JSContext*, JS::HandleValue);
JSObject* TouchListReceiver(JSContext*, JS::HandleValue);
bool TouchValueConstructor(JSContext*, unsigned, JS::Value*);
bool TouchConstructorPrototype(JSContext*, const JS::CallArgs&, bool event,
                               JS::MutableHandleObject);
bool InstallTouchInterfaces(State&);
bool CaptureTouchExposure(State&);
bool ApplyTouchExposure(State&, bool);
const js::BaseProxyHandler* TouchListHandler();
bool NewTouchList(State&, JS::MutableHandleObject);
bool TouchListLength(JSContext*, JS::HandleObject, uint32_t*);
bool TouchListItem(JSContext*, JS::HandleObject, uint32_t, JS::MutableHandleValue);
bool AppendTouch(JSContext*, JS::HandleObject, JS::HandleValue);
bool ConvertTouchSequence(State&, JS::HandleValue, JS::MutableHandleObject);
bool InitTouchFields(JSContext*, JS::HandleObject, JS::HandleObject);
bool BuildTouchMembers(State&, const JS::HandleValueArray&, JS::MutableHandleObject);
bool TouchEventList(JSContext*, Event*, uint32_t, JS::MutableHandleObject);
bool InitTouchEvent(JSContext*, unsigned, JS::Value*);
// Original dispatch integration supplies target objects; these functions never
// read public DOM tree properties or create an independent route.
bool PrepareTouchTargetList(JSContext*, Event*, JS::HandleObject);
}
