#include "resource_network/state.h"
#include "resource_network/cache.h"
#include "resource_network/webtransport.h"
#include "resource_network/async_http.h"
#include "resource_network/callback_entry.h"
#include "resource_network/abort.h"
#include "resource_network/native_fetch.h"
#include "resource_network/native_fetch_internal.h"
#include "js/GlobalObject.h"
#include "js/Wrapper.h"
#include "resource_network/transform_stream.h"
#include "resource_network/writable_stream.h"
#include "interaction/realm.h"
#include <new>
#include "js/Object.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
CallbackResult CheckCallbackEntry(State& state,JS::HandleValue callback,CallbackContext context) {
  if (callback.isUndefined()) return CallbackResult::Success;
  JSContext* cx=state.cx;
  JS::RootedObject target(cx,callback.isObject()?js::CheckedUnwrapStatic(&callback.toObject()):nullptr);
  if (!target) {Error(cx,"Callback realm is unavailable");return CallbackResult::Failure;}
  JS::RootedObject global(cx,JS::GetNonCCWObjectGlobal(target));
  if (!state.callbackEntry || state.callbackEntry(state.callbackEntryContext,global))
    return CallbackResult::Success;
  JS::RootedValue error(cx);
  State& exceptionOwner=context.exceptionOwner?*context.exceptionOwner:state;
  if (!AbortException(exceptionOwner,"NotSupportedError",&error,context.message)) return CallbackResult::Failure;
  JS_SetPendingException(cx,error);
  return CallbackResult::EntryRejected;
}
void Realm::SetCallbackEntryPort(CallbackEntryPort port,void* context) {
  if (state_) {state_->callbackEntry=port;state_->callbackEntryContext=context;}
}
static const JSErrorFormatString* ErrorFormat(void* data, unsigned) {
  return static_cast<const JSErrorFormatString*>(data);
}
bool Error(JSContext* cx, const char* message, bool range) {
  JSErrorFormatString format{"RUYIJS_RESOURCE", "{0}", 1,
                            int16_t(range ? JSEXN_RANGEERR : JSEXN_TYPEERR)};
  JS_ReportErrorNumberASCII(cx, ErrorFormat, &format, 0, message);
  return false;
}
bool Report(JSContext* cx, Status status) {
  if (status.code == ErrorCode::None) return true;
  if (status.code == ErrorCode::OutOfMemory) { JS_ReportOutOfMemory(cx); return false; }
  return Error(cx, status.message, status.code == ErrorCode::Range);
}
// One ordered installer sequence, run once for the owning realm and once per
// child realm against that realm's own PageHelpers and Interaction owners. Each
// realm ends up with its own Headers/Response/Request/Abort objects on its own
// global; the resource catalog, the NetLog and the Web Storage owner behind
// them stay the ones this Realm owns.
static bool IsStorageInstance(JSObject* object) {
  return JS::GetClass(object) == &kStorageClass &&
         !JS::GetReservedSlot(object, 0).isUndefined() &&
         JS::GetReservedSlot(object, 1).isInt32();
}
static bool InstallSurface(State& s, PageHelpers& helpers, interaction::Realm& events) {
  if (!Report(s.cx, NewArena(&s.arena))) return false;
  JSAutoRealm realm(s.cx, s.page);
  events.SetStorageBrand(IsStorageInstance);
  // The AbortSignal prototype extends the interaction EventTarget prototype and
  // the Request factory hands every request one, so the signal interface is
  // installed before the request interface.
  return InstallQueuingStrategies(s, helpers) && InstallReadableStream(s, helpers) &&
         InstallHeaders(s, helpers) && InstallResponse(s, helpers) &&
         InstallAbort(s, helpers, events) && InstallWritableStream(s, helpers) &&
         InstallTransformStreams(s, helpers) && InstallRequest(s, helpers) &&
         InstallCache(s, helpers, s.page) && InstallWebTransport(s, helpers) && InstallPage(s, helpers);
}
Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers)
    : state_(new(std::nothrow) State(cx, page, helpers)) {
  if (!state_) JS_ReportOutOfMemory(cx);
}
Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, Realm& source)
    : state_(new(std::nothrow) State(cx, page, helpers)) {
  if (!state_) { JS_ReportOutOfMemory(cx); return; }
  if (!source.state_) { state_.reset(); return; }
  // Same origin by construction: a child browsing context inherits its
  // creator's origin, so it shares the creator's Web Storage owner. The
  // resource model becomes a view over the source's one aggregate.
  state_->store = source.state_->store;
  state_->owns_store = false;
  state_->beaconBudget = source.state_->beaconBudget;
  state_->xhrTask = source.state_->xhrTask;
  state_->xhrTaskContext = source.state_->xhrTaskContext;
  state_->xhrCheckpoint = source.state_->xhrCheckpoint;
  state_->userInput = source.state_->userInput;
  state_->keepaliveTask = source.state_->keepaliveTask;
  state_->keepaliveTaskContext = source.state_->keepaliveTaskContext;
  state_->callbackEntry=source.state_->callbackEntry;
  state_->callbackEntryContext=source.state_->callbackEntryContext;
  state_->abortDelay=source.state_->abortDelay;
  state_->abortDelayContext=source.state_->abortDelayContext;
  state_->cookieTask=source.state_->cookieTask;
  state_->cookieTaskContext=source.state_->cookieTaskContext;
  state_->transportExceptions=source.state_->transportExceptions;
  if (!Report(cx, state_->resources.ShareFrom(source.state_->resources))) state_.reset();
}
Realm::~Realm() {
  // Model/factories remain alive throughout detachment and target release.
  if (state_) {
    RetireTransports(*state_);
    DetachNativeFetches(*state_);
    DetachAsyncHttpSource(state_->asyncSource);
  }
}
bool Realm::BindAsyncHttp(AsyncHttp& http) {
  if (!state_) return false;
  if (state_->asyncSource) return true;
  return Report(state_->cx, http.OpenSource(state_->resources, &state_->asyncSource));
}
std::shared_ptr<AsyncHttpSource> Realm::AsyncSource() const {
  return state_ ? state_->asyncSource : nullptr;
}
bool Realm::InstallCancellation(PageHelpers& helpers, interaction::Realm& events) {
  if (!state_) return false;
  JSAutoRealm realm(state_->cx, state_->page);
  return InstallAbort(*state_, helpers, events);
}
bool Realm::InstallWorkerStreams(PageHelpers& helpers) {
  if (!state_) return false;
  JSAutoRealm realm(state_->cx, state_->page);
  return InstallQueuingStrategies(*state_, helpers) && InstallReadableStream(*state_, helpers) &&
         InstallWritableStream(*state_, helpers) && InstallTransformStreams(*state_, helpers);
}
bool Realm::Install(PageHelpers& helpers, interaction::Realm& events) {
  if (!state_ || !Report(state_->cx, state_->resources.Initialize())) return false;
  return InstallSurface(*state_, helpers, events);
}
bool Realm::InstallInto(PageHelpers& helpers, interaction::Realm& events) {
  return state_ && InstallSurface(*state_, helpers, events);
}
Model& Realm::Resources() { return state_->resources; }
void Realm::SetAbortDelayPort(AbortDelayPort port,void* context){
 if(state_){state_->abortDelay=port;state_->abortDelayContext=context;}
}
void Realm::SetTransportExceptionPorts(const TransportExceptionPorts& ports) {
  if(state_) state_->transportExceptions=ports;
}
void Realm::RetireNativeFetches() { if (state_) resource_network::RetireNativeFetches(*state_); }
void Realm::RetireWebTransports() { if (state_) RetireTransports(*state_); }
void Realm::SetXhrTaskPort(XhrTaskPort port, void* context, XhrCheckpoint checkpoint) {
  if (!state_) return;
  state_->xhrTask = port;
  state_->xhrTaskContext = context;
  state_->xhrCheckpoint = checkpoint;
}
void Realm::SetUserInputPort(bool (*port)()) { if (state_) state_->userInput = port; }
void Realm::SetCookieTaskPort(XhrTaskPort port, void* context) {
  if (!state_) return;
  state_->cookieTask = port;
  state_->cookieTaskContext = context;
}
bool Realm::ConfigureExposure() {
  if (!state_) return false;
  JSContext* cx = state_->cx;
  JSAutoRealm realm(cx, state_->page);
  JS::RootedValue secure(cx);
  return JS_GetProperty(cx, state_->page, "isSecureContext", &secure) &&
         state_->secure.Apply(secure.isTrue());
}
void Realm::SetKeepaliveTaskPort(XhrTaskPort port, void* context) {
  if (!state_) return;
  state_->keepaliveTask = port;
  state_->keepaliveTaskContext = context;
}
void Realm::SetApiMonitor(ApiMonitorPort port, void* context) {
  if (!state_) return;
  state_->apiMonitor = port;
  state_->apiMonitorContext = context;
}
HeaderRecord* Receiver(JSContext* cx, const JS::CallArgs& args) {
  if (args.thisv().isObject() && JS::GetClass(&args.thisv().toObject()) == &kHeadersClass) {
    auto slot = JS::GetReservedSlot(&args.thisv().toObject(), 0);
    if (!slot.isUndefined()) return static_cast<HeaderRecord*>(slot.toPrivate());
  }
  Error(cx, "Headers member called on incompatible receiver");
  return nullptr;
}
}
