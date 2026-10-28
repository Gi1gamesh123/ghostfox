#ifndef RUYIJS_BROWSER_ENVIRONMENT_HISTORY_H
#define RUYIJS_BROWSER_ENVIRONMENT_HISTORY_H
#include "js/CallArgs.h"
#include "js/TypeDecls.h"
#include <cstdint>
#include <vector>
namespace ruyijs::browser_environment {
struct State;
// Slot ids for the History session-history surface installed on
// History.prototype. One JSNative dispatches on the slot, so the installer
// table stays in one place and every member keeps exactly one production
// installer.
enum HistoryMember : int32_t {
  kHistoryPushState = 400, kHistoryReplaceState, kHistoryGo, kHistoryBack,
  kHistoryForward, kHistoryState, kHistoryScrollRestoration
};
// Installs the session-history methods and the `state` accessor on the realm's
// History prototype. The `length` accessor is installed by the same factory.
bool InstallHistoryMembers(State& state, JS::HandleObject prototype);
bool DeliverPopState(JSContext* cx, unsigned argc, JS::Value* vp);
bool HistoryMethod(JSContext* cx, unsigned argc, JS::Value* vp);
// The engine structured-clone round trip shared by History and Navigation state.
bool EncodeState(State& state, JS::HandleValue value, std::vector<uint8_t>* bytes);
bool DecodeState(JSContext* cx, const std::vector<uint8_t>& bytes, JS::MutableHandleValue result);
bool HistoryStateGetter(JSContext* cx, unsigned argc, JS::Value* vp);
bool HistoryScrollRestorationGetter(JSContext* cx, unsigned argc, JS::Value* vp);
bool HistoryScrollRestorationSetter(JSContext* cx, unsigned argc, JS::Value* vp);
}  // namespace ruyijs::browser_environment
#endif