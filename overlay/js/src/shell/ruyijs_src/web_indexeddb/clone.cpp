#include "web_indexeddb/state.h"
#include "js/Exception.h"
#include "js/StructuredClone.h"
namespace ruyijs::web_indexeddb {
bool CloneWrite(State& s,JS::HandleValue value,std::vector<uint8_t>* bytes) {
  JSAutoStructuredCloneBuffer buffer(JS::StructuredCloneScope::DifferentProcess,nullptr,nullptr);
  if(!buffer.write(s.cx,value,nullptr,nullptr)) {
    JS_ClearPendingException(s.cx);return Throw(s,"DataCloneError");
  }
  if(buffer.data().Size()>1024*1024)return Throw(s,"QuotaExceededError");
  buffer.data().ForEachDataChunk([&](const char* data,size_t size){
    bytes->insert(bytes->end(),reinterpret_cast<const uint8_t*>(data),
                  reinterpret_cast<const uint8_t*>(data)+size);return true;
  });return true;
}
bool CloneRead(State& s,const std::vector<uint8_t>& bytes,JS::MutableHandleValue out) {
  JSStructuredCloneData data(JS::StructuredCloneScope::DifferentProcess);
  if(!data.Init(bytes.size()) ||
     !data.AppendBytes(reinterpret_cast<const char*>(bytes.data()),bytes.size())) {
    JS_ReportOutOfMemory(s.cx);return false;
  }
  JS::CloneDataPolicy policy;
  return JS_ReadStructuredClone(s.cx,data,JS_STRUCTURED_CLONE_VERSION,
      JS::StructuredCloneScope::DifferentProcess,out,policy,nullptr,nullptr);
}
}
