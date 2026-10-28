#include "resource_network/webtransport.h"

namespace ruyijs::resource_network {
// The wrapper owns this record. State's intrusive membership never roots JS.
// Retirement detaches every borrowed owner before stopping native work.
struct TransportLifetime {
  State* owner=nullptr;
  TransportLifetime* previous=nullptr;
  TransportLifetime* next=nullptr;
  WebTransportSession session;
  WebTransportEvent statistics;
  bool hasStatistics=false;
};
namespace {
TransportLifetime* Lifetime(JSObject* object) {
  const auto value=JS::GetReservedSlot(object,WTNative);
  return value.isUndefined()?nullptr:static_cast<TransportLifetime*>(value.toPrivate());
}
void Detach(TransportLifetime* record) noexcept {
  if (!record->owner)return;
  if (record->previous)record->previous->next=record->next;
  else record->owner->transports=record->next;
  if (record->next)record->next->previous=record->previous;
  record->owner=nullptr;record->previous=nullptr;record->next=nullptr;
}
}
WebTransportSession* TransportSession(JSObject* object) {
  auto* record=Lifetime(object);return record?&record->session:nullptr;
}
void TransportRememberStats(JSObject* object,const WebTransportEvent& event) {
  auto* record=Lifetime(object);if(!record || event.statistics.empty())return;
  record->statistics=event;record->hasStatistics=true;
}
const WebTransportEvent* TransportFinalStats(JSObject* object) {
  auto* record=Lifetime(object);return record && record->hasStatistics?&record->statistics:nullptr;
}
bool TransportAllocate(State& state,JS::HandleObject object,const WebTransportOptions& options,std::string* error) {
  auto record=std::make_unique<TransportLifetime>();
  if (!record->session.Begin(options,error))return false;
  record->owner=&state;record->next=state.transports;
  if (record->next)record->next->previous=record.get();
  state.transports=record.get();JS::SetReservedSlot(object,WTNative,JS::PrivateValue(record.release()));return true;
}
void TransportFinalize(JSObject* object) noexcept {
  auto* record=Lifetime(object);if(!record)return;
  Detach(record);delete record;
}
void RetireTransports(State& state) noexcept {
  while(state.transports) {auto* record=state.transports;Detach(record);record->session.Stop();}
}
}
