#pragma once
#include "resource_network/model_cache.h"
#include "resource_network/readable_stream.h"
#include "resource_network/state.h"

namespace ruyijs::resource_network {
enum CacheSlot { CNative, CFactories, CSlots };
enum class CacheMethod { Match, MatchAll, Add, AddAll, Put, Delete, Keys };
enum class CacheStorageMethod { Match, Has, Open, Delete, Keys };
extern const JSClass kCacheClass, kCacheStorageClass;
bool InstallCache(State&,PageHelpers&,JS::HandleObject target);
bool OpenRealmCacheScope(State&,CacheScope**);
bool CacheCall(JSContext*,unsigned,JS::Value*);
bool CacheStorageCall(JSContext*,unsigned,JS::Value*);
bool CacheBindingError(JSContext*,const char* iface,const char* method,const char* detail);
bool CacheRejectPending(JSContext*,JS::MutableHandleValue);
bool CacheRejectPromise(JSContext*,JS::HandleObject);
State* CacheState(JSContext*,JS::HandleObject);
bool CacheRequestInfo(State&,JS::HandleValue,RequestDraft*,std::string*,bool*);
bool CacheResolveRequest(State&,RequestDraft*,const std::string&,bool,const char* method,const char* iface="Cache");
bool CacheOptions(JSContext*,JS::HandleValue,CacheStorageQueryDTO*,bool storage,const char* method);
bool CacheName(JSContext*,JS::HandleValue,std::u16string*);
bool CacheResponseRead(State&,JS::HandleObject,CacheResponseDTO*);
bool CacheResponseNew(State&,const CacheResponseDTO&,JS::MutableHandleObject);
bool CacheNew(State&,CacheHandle*,JS::MutableHandleObject);
bool CacheQueryCall(State&,JS::HandleObject,const JS::CallArgs&,CacheMethod,const CacheQueryDTO&);
bool CachePutCall(State&,JS::HandleObject,const JS::CallArgs&,RequestDraft,JS::HandleObject);
bool CacheAddCall(State&,JS::HandleObject,const JS::CallArgs&,bool all);
bool CacheValidateRequest(JSContext*,const RequestDraft&,const char*);
bool CacheValidateResponse(JSContext*,const CacheResponseDTO&,const char*);
bool CacheReport(JSContext*,Status);
inline CacheHandle* CacheNative(JSObject* object) {
  return static_cast<CacheHandle*>(JS::GetReservedSlot(object,CNative).toPrivate());
}
inline CacheScope* CacheStorageNative(JSObject* object) {
  return static_cast<CacheScope*>(JS::GetReservedSlot(object,CNative).toPrivate());
}
}
