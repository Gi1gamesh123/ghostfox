#ifndef RUYIJS_HOST_BRIDGE_H
#define RUYIJS_HOST_BRIDGE_H
#include "runtime/runtime.h"
#include <cstdint>
#include <memory>
#include <vector>
#include "js/CallArgs.h"
#include "js/GCVector.h"

namespace ruyijs {
class Pipe;
class PageHelpers;
class HostBridge final {
 public:
  HostBridge(JSContext* cx, JS::HandleObject page, JS::HandleObject codec,
               Pipe& pipe, Checkpoint checkpoint, PageHelpers& helpers);
  ~HostBridge();
  HostBridge(const HostBridge&) = delete;
  HostBridge& operator=(const HostBridge&) = delete;
  bool Expose(JS::HandleString name);
  // Allocate in the caller's current object realm; Expose and live-value
  // arguments share this factory and the existing callback protocol.
  bool MakeCallback(JS::HandleString key, JS::HandleString name, JS::MutableHandleObject result);
  bool Set(JS::HandleString name, JS::HandleValue encoded);
  bool ExposeData(JS::HandleString name, JS::HandleValue encoded);
  bool NativeWrap(JS::HandleString name, JS::HandleString source, bool constructor);
  // A new top-level document (docs/owners/TOP_LEVEL.md): rebinds to its global
  // and installs every expose/native/expose_data registration again before
  // the document's scripts run, as Playwright's exposeFunction survives a
  // navigation. `set` stays a one-document global and is not replayed.
  bool Activate(JS::HandleObject page, PageHelpers& helpers);
  bool healthy() const { return healthy_; }
 private:
  enum class Kind { Function, Native, Data };
  struct Registration {
    explicit Registration(JSContext* cx) : name(cx), code(cx), value(cx) {}
    Kind kind = Kind::Function;
    JS::PersistentRootedString name, code;
    JS::PersistentRootedValue value;
    bool constructor = false;
  };
  // Keyed by name: a function/native registration replaces the global of the
  // same name, a data registration the same `__ruyijs__.data` entry.
  bool Record(Kind kind, JS::HandleString name, JS::HandleString code, bool constructor,
              JS::HandleValue value);
  std::vector<std::unique_ptr<Registration>> registrations_;
  bool replaying_ = false;
  JSContext* cx_;
  JS::PersistentRootedObject page_;
  JS::PersistentRootedObject codec_;
  JS::PersistentRootedObject data_;
  PageHelpers* helpers_;
  JS::PersistentRooted<JS::GCVector<JSObject*>> functions_;
  Pipe& pipe_;
  Checkpoint checkpoint_;
  uint64_t nextCallId_ = 0;
  bool healthy_ = true;
  bool inFlight_ = false;
  bool Name(JS::HandleString name, JS::MutableHandleId id,
              JS::MutableHandleString localName);
  bool Call(const JS::CallArgs& args);
  bool EncodeEvent(const JS::CallArgs& args, JS::HandleString name,
                     uint64_t callId, std::string* frame);
  bool ReadReply(uint64_t callId, JS::MutableHandleValue value);
  bool PageException();
  bool RegisterFunction(JS::HandleObject function, JS::HandleString name);
  static bool Native(JSContext* cx, unsigned argc, JS::Value* vp);
};
}
#endif
