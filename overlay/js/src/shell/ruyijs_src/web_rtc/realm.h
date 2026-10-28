#ifndef RUYIJS_WEB_RTC_REALM_H
#define RUYIJS_WEB_RTC_REALM_H
#include <memory>
#include "js/RootingAPI.h"
struct JSContext;
namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::web_rtc {
struct State;
// The RTCRtpScriptTransformer brand (RTCTransformEventInit.transformer), answered
// for Interaction's brand port by the runtime worker assembly.
bool IsScriptTransformer(JSObject* object);
class Realm final {
 public:
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
        interaction::Realm& events);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  bool Install();
  // A DedicatedWorker realm's members of this owner (install.cpp).
  bool InstallWorker();
  // The WebIDL `Worker` check RTCRtpScriptTransform's constructor asks the
  // Workers owner; bound by the runtime coordinator, cleared before teardown.
  // While unbound, every argument fails the check.
  using WorkerCheckPort = bool (*)(JSObject*);
  void SetWorkerCheck(WorkerCheckPort port);
  // RTCError extends DOMException: instances use the DOMException owner's one
  // native storage (create) and this owner reads its own fields back (data).
  // Bound by the runtime coordinator before Install; RTCError is not installed
  // while they are unbound.
  struct ExceptionPorts {
    bool (*create)(JSContext*, JS::HandleObject prototype, JS::HandleString message,
                   const char* name, JS::HandleValue data, JS::MutableHandleValue result) = nullptr;
    bool (*data)(JSObject* object, JS::MutableHandleValue data) = nullptr;
  };
  void SetExceptionPorts(const ExceptionPorts& ports);
  // Delayed tasks for ICE gathering (docs/owners/WEB_RTC.md); cleared before teardown.
  void SetTaskPort(bool (*port)(void*, JS::HandleObject, double), void* context);
  // The Media owner's remote MediaStreamTrack factory
  // (web_media::Realm::NewRemoteTrack): every RTCRtpReceiver is created with
  // one. Bound by the runtime coordinator, cleared before teardown.
  using RemoteTrackPort = bool (*)(void*, bool video, JS::MutableHandleObject track);
  void SetRemoteTrackPort(RemoteTrackPort port, void* context);
 private:
  std::unique_ptr<State> state_;
};
// The environment's "rtc" group port; `context` is the JSContext.
bool ConfigureProfileGroup(void* context, JS::HandleObject group);
}
#endif
