#ifndef RUYIJS_RESOURCE_REALM_H
#define RUYIJS_RESOURCE_REALM_H
#include <memory>
#include <string>
#include <string_view>
#include "resource_network/model.h"
#include "js/RootingAPI.h"

namespace ruyijs::web_platform { struct RequestBody; }
namespace ruyijs::interaction { class Realm; }

namespace ruyijs { class PageHelpers; }
namespace ruyijs::resource_network {
struct State;
class CacheSession;
class WorkerFetchSession;
class AsyncHttp;
struct AsyncHttpSource;
// Debug-session port, mirroring document_model::ApiMonitorPort. The owner
// reports one access to a member its own installer registered; the access kind
// travels as a plain int so Resource Network keeps no observability dependency.
// Null outside debug mode, and every call site then skips.
enum class ApiAccess : int { Read = 0, Write = 1, Call = 2, Construct = 3 };
using ApiMonitorPort = void (*)(void* context, const char* iface,
                                const char* member, int type);
using XhrTaskPort = bool (*)(void*, JS::HandleObject);
using XhrCheckpoint = void (*)(JSContext*);
using AbortDelayPort = bool (*)(void*,JS::HandleObject,double);
using CookieDocumentPort=bool (*)(void*,JS::HandleValue,std::string*);
using CookieClockPort=double (*)(void*);
// Borrowed runtime query over the original Scheduling retirement authority.
using CallbackEntryPort=bool (*)(void*,JS::HandleObject);
class Realm final {
 public:
  struct TransportExceptionPorts {
    bool (*create)(JSContext*,JS::HandleObject,JS::HandleString,const char*,
                   JS::HandleValue,JS::MutableHandleValue)=nullptr;
    bool (*data)(JSObject*,JS::MutableHandleValue)=nullptr;
  };
  void SetTransportExceptionPorts(const TransportExceptionPorts&);
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers);
  // Realm-local view over an existing realm: the child browsing context gets
  // its own constructors, prototypes and page globals from the same ordered
  // installers, but reads and writes the one resource catalog, the one NetLog
  // and (same origin) the one Web Storage owner the source realm owns. The
  // source realm must outlive this one.
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, Realm& source);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  bool Install(PageHelpers& helpers, interaction::Realm& events);
  // Cancellation-only exposure, using this realm's original Abort installer.
  // No resource catalog, HTTP surface or Window storage is allocated here.
  bool InstallCancellation(PageHelpers& helpers, interaction::Realm& events);
  // Worker startup needs the realm-local ReadableStream/pipeTo contract, but
  // fetch/XHR/Page require ports that are not part of the child bootstrap.
  bool InstallWorkerStreams(PageHelpers& helpers);
  // A Worker realm's Headers/Response/Request and its WorkerGlobalScope fetch,
  // from the same installers, after InstallCancellation/InstallWorkerStreams.
  // The realm owns no catalog, NetLog or Cookie jar: `exchange` carries each
  // request to the parent Context's owner (worker_network_wire.h), and the
  // parent's HttpContext is read once through it here.
  using WorkerExchange = bool (*)(void*, std::string_view request, std::string* reply);
  bool InstallWorkerNetwork(PageHelpers& helpers, JS::HandleObject globalScopePrototype,
                            WorkerExchange exchange, void* context);
  // XMLHttpRequest (no responseXML: Firefox's Worker XHR has no document) and
  // WebSocket from the Window's installers, after InstallWorkerNetwork; XHR
  // requests take the same remote port and the bound XHR task port.
  bool InstallWorkerTransports(PageHelpers& helpers, bool xhr = true);
  // importScripts: one GET of a classic script through the same PerformHttp
  // path (remote to the parent Context), `input` resolved against `base`.
  // *ok is false for an invalid URL, a network error or a non-2xx status.
  bool FetchWorkerScript(std::string_view base, std::string_view input, std::string* url,
                         std::string* source, bool* ok);
  // Parent side: one network frame from a Worker this realm's document
  // created (a context read or one whole request), answered from this realm's
  // model by the same PerformHttp a Window fetch uses.
  static bool IsWorkerNetworkFrame(std::string_view frame) {
    return !frame.empty() && (frame[0] == 'N' || frame[0] == 'C');
  }
  bool WorkerNetworkRequest(std::string_view frame, std::string* reply);
  // The same installer sequence as Install, run against a child realm's own
  // PageHelpers / Interaction owners and this realm's own global.
  bool InstallInto(PageHelpers& helpers, interaction::Realm& events);
  Model& Resources();
  void SetCacheOriginPort(bool (*)(void*,std::string*),void*);
  std::shared_ptr<CacheSession> OpenCacheSession();
  std::shared_ptr<WorkerFetchSession> OpenWorkerFetchSession();
  // One source identity for this original Model, detached before Realm teardown.
  bool BindAsyncHttp(AsyncHttp&);
  // That bound source (null before binding); requests started on it run in this
  // Model's request context and are dropped when this Realm detaches it.
  std::shared_ptr<AsyncHttpSource> AsyncSource() const;
  // Debug-session port: reports accesses to the Headers/Request/Response/
  // AbortSignal/AbortController/XHR/Storage/fetch members this owner
  // registered, for every realm it installed into. Bound by the shared
  // coordinator in debug mode and cleared before the owner is destroyed.
  void SetApiMonitor(ApiMonitorPort port, void* context);
  void SetXhrTaskPort(XhrTaskPort, void*, XhrCheckpoint);
  // Interaction's HandlingUserInput: an XHR sent during user input is
  // urgent-start ("Priority: u=0"), as Firefox marks it. A child realm copies it.
  void SetUserInputPort(bool (*)());
  // Port for work that outlives its document (beacon delivery); a child realm
  // copies it from its source like the XHR port.
  void SetKeepaliveTaskPort(XhrTaskPort, void*);
  void SetAbortDelayPort(AbortDelayPort,void*);
  // A ReadableStream (byte source) over a copy of `bytes`, from this realm's stream
  // owner; Blob.stream() reaches it through a runtime port (byte_stream_source.cpp).
  bool NewByteStream(std::string_view bytes, JS::MutableHandleObject stream);
  void SetCallbackEntryPort(CallbackEntryPort,void*);
  void RetireBodyConsumers();
  void RetireNativeFetches();
  void RetireWebTransports();
  void SetHttpEnvironment(HttpContextPort,CookieDocumentPort,void*,CookieClockPort,void*);
  // Realm task port for the queued CookieChangeEvent of window.cookieStore; a
  // child realm copies the owning realm's binding like the XHR one.
  void SetCookieTaskPort(XhrTaskPort, void*);
  // [SecureContext] window.cookieStore and CookieStore, removed or restored for
  // this realm's current secure-context state (runtime/secure_exposure.cpp).
  bool ConfigureExposure();
  bool Beacon(JSContext*, const std::string& base, const std::string& documentURL,
              const std::string& input, web_platform::RequestBody&&, bool*);
 private:
  std::unique_ptr<State> state_;
};
bool Report(JSContext* cx, Status status);
}
#endif
