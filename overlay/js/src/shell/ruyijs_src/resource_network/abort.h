#ifndef RUYIJS_RESOURCE_ABORT_H
#define RUYIJS_RESOURCE_ABORT_H
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Object.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::resource_network {
struct State;
enum AbortSlot { AAborted,AReason,AOwner,AHandler,ADependents,ASources,AListener,AAlgorithms,ALifetime,ASlots };
bool AttachAbortSignal(State&,JS::HandleObject);
void DetachAbortSignals(State&);
bool AddAbortAlgorithm(JSContext*,JS::HandleValue,JS::HandleValue);
bool RemoveAbortAlgorithm(JSContext*,JS::HandleValue,JS::HandleValue);
bool RunAbortAlgorithms(JSContext*,JS::HandleObject);
bool UnwrapAbort(JS::HandleValue,JS::MutableHandleObject);
bool AbortMember(JSContext*,unsigned,JS::Value*);
bool AbortStaticMethod(JSContext*,unsigned,JS::Value*);
bool AbortCompose(State&,JS::HandleValue,JS::MutableHandleObject);
bool AbortTimeout(State&,JS::HandleValue,JS::MutableHandleObject);
bool AbortException(State&,const char*,JS::MutableHandleValue,const char* message=nullptr);

// AbortSignal and AbortController. The signal is an EventTarget subclass whose
// abort event travels the existing interaction dispatch tree, so the resource
// domain keeps no event state of its own: the object holds only the aborted
// flag and the reason.
bool NewAbortSignal(State&, JS::MutableHandleObject result);
bool IsAbortSignal(JS::HandleValue value);
bool AbortSignalled(JS::HandleValue value);
// The reason of an aborted signal, or a fresh AbortError DOMException when the
// signal was aborted without one.
bool AbortReason(State&, JS::HandleValue value, JS::MutableHandleValue result);
bool AbortDefaultReason(State&, JS::MutableHandleValue result);
// The one abort transition: set the flag and reason, then fire the abort event.
bool AbortSignalAbort(State&, JS::HandleObject signal, JS::HandleValue reason);
// Installs the signal interface and returns its constructor; the controller
// installer and the page share it.
bool InstallAbortSignal(State&, PageHelpers&, interaction::Realm& events,
                        JS::MutableHandleObject result);
bool InstallAbort(State&, PageHelpers&, interaction::Realm& events);
}  // namespace ruyijs::resource_network
#endif
