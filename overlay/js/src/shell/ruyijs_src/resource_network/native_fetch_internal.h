#pragma once
#include "resource_network/native_fetch.h"
#include "resource_network/async_http.h"
#include "resource_network/readable_stream.h"
#include "resource_network/request_value.h"
#include <memory>

namespace ruyijs::resource_network {
enum FetchSlot { FTarget,FFactories,FGlobal,FSignal,FAlgorithm,FSteps,FRecord,FOperationSlots };
struct NativeFetchRecord : std::enable_shared_from_this<NativeFetchRecord> {
  JSContext* cx=nullptr;
  JS::PersistentRootedObject operation;
  std::shared_ptr<AsyncHttpSource> source;
  AsyncHttpTicket ticket;
  NativeFetchRecord* previous=nullptr;
  NativeFetchRecord* next=nullptr;
  // Request metadata only; the original AsyncHttp job owns request body bytes.
  std::string method,url;
  std::string mode;
  bool credentialsInclude=false;
  uint64_t workerId=0;
};
extern const JSClass kNativeFetchClass;
Status PrepareNativeFetch(const RequestDraft&,const HttpContext&,
                         std::shared_ptr<NativeFetchRecord>*,HttpOptions*) noexcept;
void AttachNativeFetch(State&,const std::shared_ptr<NativeFetchRecord>&,JS::HandleObject);
void ClearNativeFetch(State&,NativeFetchRecord&,JS::HandleObject);
bool FinishNativeFetch(State&,const std::shared_ptr<NativeFetchRecord>&,
                       NativeFetchOutcome,const HttpResult* response=nullptr);
void DiscardNativeFetch(State&,const std::shared_ptr<NativeFetchRecord>&);
bool FailNativeFetchConsumer(JSContext*,JS::HandleObject,const NativeFetchSteps&);
bool InstallNativeFetchAbort(State&,JS::HandleObject);
bool NativeFetchAbort(JSContext*,unsigned,JS::Value*);
void NativeFetchComplete(void*,AsyncHttpResult&&) noexcept;
bool StartWorkerFetch(State&,const std::shared_ptr<NativeFetchRecord>&,
                      const RequestDraft&,const HttpOptions&,RedirectMode);
void CancelNativeFetchTransport(State&,NativeFetchRecord&);
inline NativeFetchRecord* FetchRecord(JSObject* operation) {
  auto value=JS::GetReservedSlot(operation,FRecord);
  return value.isUndefined()?nullptr:static_cast<NativeFetchRecord*>(value.toPrivate());
}
}
