// Original State keeps the current manager; runtime lends lifecycle capabilities.
#include "interaction/close_watcher.h"
#include "js/Realm.h"
#include "js/String.h"
#include <cstring>

namespace ruyijs::interaction {
void Realm::SetCloseWatcherPorts(const CloseWatcherPorts& ports,void* context) {
  state_->closePorts=ports;state_->closeContext=context;
}
void Realm::NotifyCloseWatcherActivation() {
  JSObject* manager=state_->closeManager;if (!manager) return;
  if (JS::GetReservedSlot(manager,MNextAllows).isTrue()) {
    JS::SetReservedSlot(manager,MAllowed,JS::DoubleValue(JS::GetReservedSlot(manager,MAllowed).toNumber()+1));
    JS::SetReservedSlot(manager,MNextAllows,JS::BooleanValue(false));
  }
}
bool Realm::ProcessCloseWatchers() {
  JSAutoRealm realm(state_->cx,state_->page);return ProcessCloseManager(*state_);
}
bool Realm::PrepareCloseWatcherDocument(JS::MutableHandleObject result) {
  if (!state_->closeFactory) {result.set(nullptr);return true;}
  JSAutoRealm realm(state_->cx,state_->page);return NewCloseManager(state_->cx,result);
}
void Realm::CommitCloseWatcherDocument(JS::HandleObject manager) {
  ClearCloseManager(state_->closeManager);state_->closeManager=manager;
}
void Realm::RetireCloseWatchers() {ClearCloseManager(state_->closeManager);}
bool ProcessCloseInput(State& state,JS::HandleObject target,JS::HandleObject event,const char* type,bool allowed) {
  if (!allowed || std::strcmp(type,"keydown") || !state.closePorts.input) return true;
  JS::RootedValue key(state.cx,JS::GetReservedSlot(event,kEventKey));bool escape=false;
  if (!key.isString()) return true;
  JS::RootedString text(state.cx,key.toString());
  return JS_StringEqualsAscii(state.cx,text,"Escape",&escape) &&
      (!escape || state.closePorts.input(state.closeContext,target));
}
}
