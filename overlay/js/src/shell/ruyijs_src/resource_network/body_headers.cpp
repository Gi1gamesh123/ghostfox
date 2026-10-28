// MIME reads and clone snapshots use the original HeaderRecord, not JS methods.
#include "resource_network/body.h"
#include "resource_network/state.h"
#include "js/Object.h"
#include "js/Wrapper.h"

namespace ruyijs::resource_network {
namespace {
HeaderRecord* Record(JSContext* cx,JS::HandleObject headers) {
  JSObject* local=js::CheckedUnwrapStatic(headers);
  if (!local || JS::GetClass(local)!=&kHeadersClass || JS::GetReservedSlot(local,0).isUndefined()) {
    Error(cx,"Body headers require an initialized Headers object");return nullptr;
  }
  return static_cast<HeaderRecord*>(JS::GetReservedSlot(local,0).toPrivate());
}
}
bool HeaderValue(State& state,JS::HandleValue receiver,const char* name,JS::MutableHandleValue out) {
  JS::RootedObject self(state.cx,BodyReceiver(receiver));
  if (!self) return Error(state.cx,"body reader requires a Request or Response");
  JS::RootedObject headers(state.cx,&JS::GetReservedSlot(self,1).toObject());
  HeaderRecord* record=Record(state.cx,headers);
  if (!record) return false;
  std::string text;bool found=false;
  if (!Report(state.cx,Get(record,name,&text,&found))) return false;
  if (!found) {out.setNull();return true;}
  return Value(state.cx,text,out);
}
bool CopyBodyHeaders(State& state,JS::HandleObject headers,JS::MutableHandleObject result) {
  HeaderRecord* record=Record(state.cx,headers);
  if (!record) return false;
  HeaderPairs pairs;
  if (!Report(state.cx,SnapshotHeaders(record,&pairs))) return false;
  return HeaderObject(state,pairs,result,HeadersImmutable(record));
}
}
