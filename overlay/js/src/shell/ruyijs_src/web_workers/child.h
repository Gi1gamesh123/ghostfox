#ifndef RUYIJS_WEB_WORKERS_CHILD_H
#define RUYIJS_WEB_WORKERS_CHILD_H
#include "runtime/runtime.h"
#include "runtime/web_kit.h"
#include "web_workers/kind.h"
#include "web_workers/clone.h"
#include <string>
#include <string_view>
#include <deque>
namespace ruyijs { class Pipe; class ExceptionCapture; }
namespace ruyijs { enum class PipeResult; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::web_workers {
// Runtime supplies the existing task owner; Child keeps no scheduling state.
struct ChildTasks {
  void* context = nullptr;
  void (*begin)(void*) = nullptr;
  bool (*pump)(void*) = nullptr;
};
// The runtime installs URL-dependent owners after the startup frame validates.
struct ChildStartup {
  void* context = nullptr;
  bool (*prepare)(void*, std::string_view scriptUrl, std::string_view environment) = nullptr;
};
// The child process uses the same Worker message codec and Interaction owner.
class Child final {
 public:
  // `kind`: the Worker global kind this process runs. Service installs only
  // WorkerGlobalScope and hands every frame after startup to the Service Worker
  // owner through the frame port; Shared installs SharedWorkerGlobalScope.
  Child(JSContext*, JS::HandleObject, PageHelpers&, interaction::Realm&, Pipe&,
        JS::HandleObject exception, WorkerKind kind = WorkerKind::Dedicated);
  using FramePort = bool (*)(void*, std::string_view);
  void SetFramePort(FramePort port, void* context) { framePort_ = port; frameContext_ = context; }
  // BroadcastChannel frames ('\x07') from the parent process.
  void SetBroadcastPort(FramePort port, void* context) { broadcastPort_ = port; broadcastContext_ = context; }
  // SharedWorker ports (child_shared.cpp): a connect frame creates a worker-side
  // MessagePort through the factory; a message/close frame goes to the matching
  // port through the bound callbacks. The child's own RelaySharedChild is bound
  // as web_messaging's remote relay so a worker-side postMessage returns to the
  // Window through this pipe.
  using RemotePortFactory = JSObject* (*)(void* context, uint32_t id);
  using RemotePortDeliver = bool (*)(void* context, uint32_t id, JS::HandleValue value);
  using RemotePortClose = void (*)(void* context, uint32_t id);
  void SetMessagePortFactory(RemotePortFactory port, void* context) { portFactory_ = port; portFactoryContext_ = context; }
  void SetMessagePortDeliver(RemotePortDeliver port, void* context) { portDeliver_ = port; portDeliverContext_ = context; }
  void SetMessagePortClose(RemotePortClose port, void* context) { portClose_ = port; portCloseContext_ = context; }
  bool RelaySharedPort(uint32_t id, JS::HandleValue value, JS::HandleValue transfer);
  void CloseSharedPort(uint32_t id);
  // Queues one frame to the parent and submits it now.
  bool Queue(std::string frame);
  WorkerKind kind() const { return kind_; }
  bool service() const { return kind_ == WorkerKind::Service; }
  bool shared() const { return kind_ == WorkerKind::Shared; }
  bool dedicated() const { return kind_ == WorkerKind::Dedicated; }
  JSObject* exceptionPrototype() const { return exception_.get(); }
  bool Install();
  bool Serve(Checkpoint, ExceptionCapture&, const ChildTasks& = {}, const ChildStartup& = {});
  JSObject* globalScopePrototype() const { return globalScope_.prototype.get(); }
  bool closed() const { return closed_; }
  bool ReportError(JS::HandleValue, JS::HandleObject);
  bool Exchange(std::string_view request, std::string* reply);
  // importScripts' classic-script fetch: resolves `input` against `base` and
  // reads the body; *ok is false for a network error or a non-OK status.
  using ScriptPort = bool (*)(void*, std::string_view base, std::string_view input, std::string* url,
                              std::string* source, bool* ok);
  void SetScriptPort(ScriptPort, void*);
  // WorkerGlobalScope.prototype own keys in Firefox's binding order, after
  // every installer that adds a WorkerGlobalScope member.
  bool OrderGlobalScope();
 private:
  static bool ScopeMethod(JSContext*, unsigned, JS::Value*);
  bool InstallScopeMethods();
  bool ImportScripts(const JS::CallArgs&);
  std::string url_;
  ScriptPort scriptPort_ = nullptr;
  void* scriptContext_ = nullptr;
  static bool Send(JSContext*, unsigned, JS::Value*);
  static bool Close(JSContext*, unsigned, JS::Value*);
  static bool GlobalMember(JSContext*, unsigned, JS::Value*);
  bool HandleSharedFrame(std::string_view frame);
  JSContext* cx_;
  WebKit kit_;
  interaction::Realm& events_;
  Pipe& pipe_;
  WebFace globalScope_, dedicatedScope_, sharedScope_;
  JS::PersistentRootedString name_;
  JS::PersistentRootedObject exception_, handlers_, errorPrototype_;
  bool closed_ = false;
  WorkerKind kind_ = WorkerKind::Dedicated;
  FramePort framePort_ = nullptr;
  void* frameContext_ = nullptr;
  FramePort broadcastPort_ = nullptr;
  void* broadcastContext_ = nullptr;
  RemotePortFactory portFactory_ = nullptr;
  void* portFactoryContext_ = nullptr;
  RemotePortDeliver portDeliver_ = nullptr;
  void* portDeliverContext_ = nullptr;
  RemotePortClose portClose_ = nullptr;
  void* portCloseContext_ = nullptr;
  bool reportingError_ = false;
  uint64_t childSequence_ = 0;
  WorkerMessageToken lastIncoming_;
  std::deque<std::string> deferred_;
  size_t deferredBytes_ = 0;
  PipeResult NextFrame(std::string*, bool*);
  bool EvaluateStartup(std::string_view, const ChildStartup&);
};
}
#endif
