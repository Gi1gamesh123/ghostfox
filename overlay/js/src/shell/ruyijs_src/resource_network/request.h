#ifndef RUYIJS_RESOURCE_REQUEST_H
#define RUYIJS_RESOURCE_REQUEST_H
#include <string>
#include "resource_network/page_http.h"
#include "resource_network/request_value.h"
#include "js/CallArgs.h"
#include "js/RootingAPI.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::resource_network {
struct State;

// RequestInit conversion is an ephemeral WebIDL dictionary snapshot, not a
// second request owner. Bits distinguish omitted fields from explicit values.
enum RequestInitField : uint32_t {
  InitBody=1<<0, InitCache=1<<1, InitCredentials=1<<2, InitHeaders=1<<3,
  InitIntegrity=1<<4, InitKeepalive=1<<5, InitMethod=1<<6, InitMode=1<<7,
  InitPriority=1<<8, InitRedirect=1<<9, InitReferrer=1<<10,
  InitReferrerPolicy=1<<11, InitSignal=1<<12
};
struct RequestInitFields { RequestDraft values; uint32_t present = 0; };
bool RequestInitInput(State&, JS::HandleValue, RequestInitFields*, JS::MutableHandleValue);
bool RequestIntegrity(JSContext*, JS::HandleValue, std::u16string*);
bool ApplyRequestInit(State&, const RequestInitFields&, RequestDraft*);
bool RequestURL(State&, const std::string&, std::string*);

// The one argument-normalization path: input is a URL string or a Request, init
// an optional dictionary. The resulting signal is an AbortSignal or null.
bool RequestInput(State&, JS::HandleValue input, JS::HandleValue init, RequestDraft*,
                  JS::MutableHandleValue signal);
// Refills a draft (and the signal) from an existing Request; used by clone().
bool ReadRequest(State&, JS::HandleObject source, RequestDraft*, JS::MutableHandleValue signal);
// Builds the object from a finished draft. A null signal gets a fresh one.
bool RequestNew(State&, JS::HandleObject prototype, const RequestDraft&, JS::HandleValue signal,
                JS::MutableHandleObject result);
bool InstallRequestObject(State&, PageHelpers&, JS::HandleObject prototype);
bool InstallRequest(State&, PageHelpers&);
}  // namespace ruyijs::resource_network
#endif
