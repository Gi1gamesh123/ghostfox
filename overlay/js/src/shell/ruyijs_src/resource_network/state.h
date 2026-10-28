#ifndef RUYIJS_RESOURCE_STATE_H
#define RUYIJS_RESOURCE_STATE_H
#include "resource_network/realm.h"
#include "resource_network/body.h"
#include "resource_network/model_headers.h"
#include "resource_network/model_storage.h"
#include "resource_network/page_http.h"
#include "runtime/secure_slot.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "js/CallArgs.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::resource_network {
struct AsyncHttpSource;
struct NativeFetchRecord;
struct BeaconBudget { size_t count = 0, bytes = 0; };
struct State {
  JSContext* cx;
  WebKit kit;
  // The realm holds the one prototype of each interface it installs; the request
  // and signal ones are filled by their installers in later slices.
  JS::PersistentRootedObject page, holder, responsePrototype, requestPrototype,
      signalPrototype, xhrPrototype, xhrUploadPrototype;
  // ResourceNetwork owns the sole stream/reader/controller factory per realm.
  // Buffered Bodies retain BodyRecord; default sources retain controller slots.
  WebFace stream, defaultReader, defaultController, byteController, byobReader, byobRequest,
      transformStream, transformController, writableStream, writableWriter, writableController,
      websocket, eventSource;
  WebFace cache, cacheStorage;
  WebFace transport,transportError,transportDatagrams,transportBidi,
      transportSend,transportReceive,transportGroup,transportDatagramsWritable;
  Realm::TransportExceptionPorts transportExceptions;
  JS::PersistentRootedObject transportExceptionPrototype;
  JS::PersistentRootedObject cacheStorageObject;
  bool (*cacheOrigin)(void*,std::string*)=nullptr;
  void* cacheOriginContext=nullptr;
  Realm::WorkerExchange cacheExchange=nullptr,workerExchange=nullptr;
  void* cacheExchangeContext=nullptr;
  void* workerExchangeContext=nullptr;
  WebFace byteLengthStrategy, countStrategy;
  JS::PersistentRootedObject byteLengthSize, countSize;
  JS::PersistentRootedValue xhrAddListener, xhrRemoveListener, xhrException;
  JS::PersistentRootedValue abortException,abortAdd,abortRemove;
  CallbackEntryPort callbackEntry = nullptr;
  void* callbackEntryContext = nullptr;
  AbortDelayPort abortDelay = nullptr;
  void* abortDelayContext = nullptr;
  struct XhrRecord* xhrRecords = nullptr;
  struct AbortLifetime* abortLifetimes = nullptr;
  struct BodyLifetime* bodyOperations = nullptr;
  std::shared_ptr<BeaconBudget> beaconBudget = std::make_shared<BeaconBudget>();
  XhrTaskPort xhrTask = nullptr;
  void* xhrTaskContext = nullptr;
  XhrCheckpoint xhrCheckpoint = nullptr;
  bool (*userInput)() = nullptr;  // Interaction's HandlingUserInput, bound by the runtime
  // Beacon delivery outlives the document that queued it (Firefox sends a
  // beacon queued during pagehide/unload), so it posts through this port when
  // the runtime bound one; the XHR port above belongs to the document.
  XhrTaskPort keepaliveTask = nullptr;
  void* keepaliveTaskContext = nullptr;
  HeaderArena* arena = nullptr;
  Model resources;
  std::shared_ptr<AsyncHttpSource> asyncSource;
  NativeFetchRecord* nativeFetches=nullptr;
  struct TransportLifetime* transports=nullptr;
  struct StorageArea* store = nullptr;
  // True for the realm that created the store; a child browsing context's
  // realm-local view shares the owning realm's store (same origin) and must
  // not destroy it, so only the owner releases it.
  bool owns_store = true;
  // True in a Worker realm: fetch reports through WorkerGlobalScope and the
  // model hands requests to the parent's owner (InstallWorkerNetwork).
  bool workerScope = false;
  // The Worker realm's exchange port and the parent HttpContext it read; the
  // model's context and remote ports point into it for the realm's lifetime.
  std::shared_ptr<void> workerLink;
  // The owning page's event tree: AbortSignal attaches to it and fires abort
  // there, so the resource domain stores no listener state.
  interaction::Realm* events = nullptr;
  // Debug-session port, bound by the shared coordinator in debug mode. Reports
  // one access to an install-registered member of this owner; a null port
  // (non-debug mode) skips without touching engine state.
  ApiMonitorPort apiMonitor = nullptr;
  HttpContextPort httpContext=nullptr;
  CookieDocumentPort cookieDocument=nullptr;
  void* httpEnvironment=nullptr;
  CookieClockPort cookieClock=nullptr;
  void* cookieClockContext=nullptr;
  JS::PersistentRootedValue cookieBrand;
  // window.cookieStore: the one CookieStore instance of this realm and the
  // [SecureContext] properties its installer registered with the exposure
  // coordinator (runtime/secure_slot.h). The change event it fires runs as a
  // realm task through the port below, which a child realm inherits.
  JS::PersistentRootedObject cookieStore;
  SecureSlots secure;
  XhrTaskPort cookieTask = nullptr;
  void* cookieTaskContext = nullptr;
  void* apiMonitorContext = nullptr;
  void Observe(const char* iface, const char* member, int type) const {
    if (apiMonitor) apiMonitor(apiMonitorContext, iface, member, type);
  }
  State(JSContext* context, JS::HandleObject global, PageHelpers& helpers)
      : cx(context), kit(context, global, helpers, this), page(context, global), holder(context), responsePrototype(context),
        requestPrototype(context), signalPrototype(context), xhrPrototype(context),
      xhrUploadPrototype(context), stream(context), defaultReader(context), defaultController(context),
      byteController(context), byobReader(context), byobRequest(context), transformStream(context), transformController(context),
      writableStream(context), writableWriter(context), writableController(context), websocket(context), eventSource(context),
      cache(context), cacheStorage(context), cacheStorageObject(context),
      transport(context),transportError(context),transportDatagrams(context),transportBidi(context),
      transportSend(context),transportReceive(context),transportGroup(context),transportDatagramsWritable(context),
      transportExceptionPrototype(context),
      byteLengthStrategy(context), countStrategy(context), byteLengthSize(context), countSize(context),
        xhrAddListener(context), xhrRemoveListener(context), xhrException(context),
        abortException(context),abortAdd(context),abortRemove(context),cookieBrand(context),
        cookieStore(context),secure(context) {}
  ~State();
};
enum class Operation { Append, Delete, Get, Cookies, Has, Set, Entries, Keys, Values, ForEach };
extern const JSClass kHeadersClass, kIteratorClass, kFactoryClass, kStorageClass, kResponseClass,
    kRequestClass, kAbortSignalClass, kAbortControllerClass, kReadableStreamClass,
    kCookieStoreClass, kWebSocketClass;
bool Error(JSContext*, const char*, bool range = false);
// The one native-function factory: slot 0 is the owning State*, the optional
// slot 1 is an operation id the callback reads back.
State* FunctionOwner(const JS::CallArgs&);
// The Headers factory object carries its State in reserved slot 3; this reads
// that slot so the Headers methods report through the same owner port.
State* FactoryOwner(const JS::CallArgs&);
// The installed member name of a Headers Operation: append/get/.../forEach.
const char* OperationName(Operation);
JSObject* NativeFunction(State&, JSNative, const char*, unsigned length, unsigned flags = 0);
JSObject* NativeSlot(State&, JSNative, const char*, unsigned length, int32_t slot);
// Promise helpers: the resolved/rejected pair behind the body readers.
bool Settled(JSContext*, JS::HandleValue, JS::MutableHandleValue);
bool Rejected(JSContext*, JS::HandleValue, JS::MutableHandleValue);
bool RejectWithTypeError(JSContext*, JS::HandleObject promise, const char*);
bool TypeErrorPromise(JSContext*, const char*, JS::MutableHandleValue);
// The init.headers conversion: a Headers object, a pair sequence or a record.
bool HeaderPairsArgument(JSContext*, JS::HandleValue, HeaderPairs*);
// The page's own location, as the base URL of a relative request.
bool PageBaseURL(State&, std::string*);
// The Headers owner builds one more object of its own class for a Request or
// Response; no second header container exists.
bool HeaderObject(State&, const HeaderPairs&, JS::MutableHandleObject, bool immutable=false);
// Response: the object layout (response_object.cpp) and the interface
// (response.cpp).
bool CreateResponse(State&,const JS::CallArgs&,int32_t,const char*,const HeaderPairs&,const char*,JS::MutableHandleObject);
bool ResponseNew(State&, JS::HandleObject prototype, BodyRecord*, JS::HandleObject headers,
                 int32_t status, JS::HandleString statusText, JS::HandleString url,
                 const char* type, bool redirected, JS::MutableHandleObject);
bool ResponseObject(State&, const HttpResult&, const std::string& url,
                    const std::string& method, const std::string& mode,
                    bool credentialsInclude, JS::MutableHandleObject);
bool InstallResponseObject(State&, PageHelpers&, JS::HandleObject prototype);
bool InstallResponse(State&, PageHelpers&);
bool InstallRequest(State&, PageHelpers&);
bool InstallAbort(State&, PageHelpers&, interaction::Realm&);
bool InstallHeaders(State&, PageHelpers&);
bool InstallReadableStream(State&, PageHelpers&);
bool InstallQueuingStrategies(State&, PageHelpers&);
bool InstallPage(State&, PageHelpers&);
bool InstallFetch(State&, PageHelpers&, JS::HandleObject target, const char* iface);
bool InstallXhr(State&, PageHelpers&);
bool InstallStorage(State&, PageHelpers&);
bool InstallCookie(State&,PageHelpers&,JS::HandleObject);
bool InstallCookieStore(State&,PageHelpers&);
bool InstallWebSocket(State&,PageHelpers&);
bool InstallEventSource(State&);
HeaderRecord* Receiver(JSContext*, const JS::CallArgs&);
bool Convert(JSContext*, JS::HandleValue, std::string*);
JSString* DecodeBodyText(JSContext*, std::string_view);
bool EncodeBodyText(JSContext*, JS::HandleValue, std::string*);
bool ConvertInit(JSContext*, JS::HandleValue, HeaderDraft*);
bool Value(JSContext*, std::string_view, JS::MutableHandleValue);
bool Tag(JSContext*, JS::HandleObject, const char*);
bool Constructor(JSContext*, unsigned, JS::Value*);
bool Method(JSContext*, unsigned, JS::Value*);
bool Iterate(JSContext*, unsigned, JS::Value*);
bool Next(JSContext*, unsigned, JS::Value*);
bool ForEach(JSContext*, unsigned, JS::Value*);
}
#endif
