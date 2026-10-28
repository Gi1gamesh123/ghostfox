#include "web_indexeddb/state.h"
#include "js/Object.h"
namespace ruyijs::web_indexeddb {
namespace {
bool Fail(Job& job,const char* name) {
  auto& s=*job.owner;JS::RootedValue error(s.cx);
  if (!ErrorValue(s,name,&error)) return false;
  JS::SetReservedSlot(job.request,ResultSlot,JS::UndefinedValue());
  JS::SetReservedSlot(job.request,ErrorSlot,error);
  JS::SetReservedSlot(job.request,ReadySlot,JS::BooleanValue(true));job.done=true;
  return s.events.Fire(job.request,"error",true,true);
}
}
bool Complete(Job& job) {
  auto& s=*job.owner;JSContext* cx=s.cx;JSAutoRealm realm(cx,s.kit.page);
  if (!job.remove && s.store->connections.size()>=4096) return Fail(job,"QuotaExceededError");
  JS::RootedObject db(cx);JS::RootedValue name(cx);
  if (!job.remove) {
    db=s.kit.New(&kDatabaseClass,s.databaseFace.prototype);
    if (!db || !String(cx,job.key.second,&name) || !s.events.Attach(db)) return false;
  }
  Result result=s.store->model.Apply(job.key,job.version,job.remove);
  if (result.code==Code::Memory) {JS_ReportOutOfMemory(cx);return false;}
  if (result.code==Code::Version) return Fail(job,"VersionError");
  if (result.code==Code::Quota) return Fail(job,"QuotaExceededError");
  if (result.code==Code::Blocked) return true;
  JS::SetReservedSlot(job.request,ReadySlot,JS::BooleanValue(true));
  if (job.remove) {
    job.done=true;
    return VersionEvent(s,job.request,"success",result.oldVersion,0,true);
  }
  JS::SetReservedSlot(db,DbOwner,JS::PrivateValue(&s));
  JS::SetReservedSlot(db,DbId,JS::Int32Value(result.connection));
  JS::SetReservedSlot(db,DbName,name);
  JS::SetReservedSlot(db,DbVersion,JS::DoubleValue(double(result.version)));
  JS::SetReservedSlot(db,8,JS::BooleanValue(true));
  JS::SetReservedSlot(db,9,JS::BooleanValue(false));
  JS::SetReservedSlot(db,DbUpgrade,JS::NullValue());
  for (int i=0;i<4;++i) JS::SetReservedSlot(db,DbHandler+i,JS::NullValue());
  s.store->connections.push_back(std::make_unique<Connection>(&s,cx,db,job.key,result.connection));
  JS::SetReservedSlot(job.request,ResultSlot,JS::ObjectValue(*db));
  if (result.version>result.oldVersion) {
    // The upgrade runs in a versionchange transaction: the open request names it
    // during upgradeneeded; it completes (or aborts) before the request's success.
    JS::RootedObject upgrade(cx,NewVersionTransaction(s,db));
    if (!upgrade) return false;
    JS::SetReservedSlot(db,DbUpgrade,JS::ObjectValue(*upgrade));
    JS::SetReservedSlot(job.request,TransactionSlot,JS::ObjectValue(*upgrade));
    if (!VersionEvent(s,job.request,"upgradeneeded",result.oldVersion,result.version,false)) return false;
    JS::SetReservedSlot(db,8,JS::BooleanValue(false));
    if (JS::GetReservedSlot(upgrade,TxActive).toInt32()==2 || !s.store->model.Active(result.connection)) {
      JS::SetReservedSlot(upgrade,TxActive,JS::Int32Value(2));
      s.store->model.Revert(result.connection);
      if (s.store->model.Active(result.connection)) s.store->model.Close(result.connection);
      JS::SetReservedSlot(job.request,TransactionSlot,JS::NullValue());
      return s.events.Fire(upgrade,"abort",true,false) && Fail(job,"AbortError");
    }
    JS::SetReservedSlot(upgrade,TxActive,JS::Int32Value(1));
    s.store->model.Settle(result.connection);
    if (!s.events.Fire(upgrade,"complete",false,false)) return false;
    JS::SetReservedSlot(job.request,TransactionSlot,JS::NullValue());
  }
  JS::SetReservedSlot(db,8,JS::BooleanValue(false));
  if (!s.store->model.Active(result.connection)) return Fail(job,"AbortError");
  job.done=true;
  return s.events.Fire(job.request,"success",false,false);
}
}
