#ifndef RUYIJS_HOST_VALUES_H
#define RUYIJS_HOST_VALUES_H
#include "runtime/runtime.h"
#include <memory>
#include "js/Value.h"
#include "js/GCVector.h"
namespace ruyijs {
class HostBridge;
// Host references only. Browser state remains in each original object's owner.
class HostValues final {
 public:
  HostValues(JSContext* cx, JS::HandleObject page, HostBridge& bridge)
      : cx_(cx), page_(cx,page), bridge_(bridge) {}
  ~HostValues() { while(entries_)Release(entries_->id); }
  bool Encode(JS::HandleValue value, bool toPython, JS::MutableHandleValue result,
              JS::HandleValue receiver = JS::UndefinedHandleValue);
  bool Request(JS::HandleObject request, JS::MutableHandleValue result);
  void Begin() { checkpoint_=next_; nodes_=0; units_=0; }
  // A new top-level document: later requests default to its global. Values
  // already handed out keep naming their objects in the retired realm.
  void Rebind(JS::HandleObject page) { page_=page; }
  void Finish(bool success);
 private:
  struct Entry {
    uint64_t id;
    JS::PersistentRootedValue value, receiver;
    std::unique_ptr<Entry> next;
    Entry(JSContext* cx,uint64_t id,JS::HandleValue v,JS::HandleValue r)
        :id(id),value(cx,v),receiver(cx,r){}
  };
  JSContext* cx_;
  JS::PersistentRootedObject page_;
  HostBridge& bridge_;
  std::unique_ptr<Entry> entries_;
  uint64_t next_=1,checkpoint_=1;
  size_t count_=0,nodes_=0,units_=0;
  Entry* Find(uint64_t id);
  bool Retain(JS::HandleValue value,JS::HandleValue receiver,JS::MutableHandleValue result);
  void Release(uint64_t id);
  bool Decode(JS::HandleValue encoded,JS::MutableHandleValue result,size_t depth=0);
  bool Visit(JS::HandleValue value,bool toPython,JS::HandleValue receiver,
             JS::MutableHandleValue result,size_t depth);
  bool Bytes(JS::HandleObject object,JS::MutableHandleValue result);
  bool Label(JS::HandleObject object,JS::MutableHandleValue result,bool functionSource=false);
  bool Budget(size_t units=0);
};
bool HostValueError(JSContext*,const char*);
bool HostValuePacket(JSContext*,const char*,JS::HandleValue,JS::MutableHandleValue);
bool HostValueIndex(JSContext*,JS::HandleValue,uint64_t*);
}
#endif
