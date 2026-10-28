// Cross-origin WindowProxy and Location (docs/owners/BROWSER_ENVIRONMENT.md,
// 2026-10-21). Every realm of a Context shares one compartment, so the HTML
// IsPlatformObjectSameOrigin checks are made per access, as Firefox's
// MaybeCrossOriginObject does: a window handed to a realm of another origin
// (contentWindow, parent, top, MessageEvent.source) is a proxy that forwards
// while the two are same-origin and otherwise exposes only the cross-origin
// properties (CrossOriginProperties), throwing SecurityError for the rest.
#ifndef RUYIJS_BROWSER_ENVIRONMENT_CROSS_ORIGIN_H
#define RUYIJS_BROWSER_ENVIRONMENT_CROSS_ORIGIN_H
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs::browser_environment {
struct State;
void RegisterWindow(State* state);
void UnregisterWindow(State* state);
State* WindowState(JSObject* global);
// True when `viewer` and `target` (window globals) are same origin; a global
// that is not a registered window (the host's own realm) sees everything.
bool SameOriginWindows(JSContext* cx, JSObject* viewer, JSObject* target, bool* same);
// `target` as `viewer` sees it: itself when same origin, else its one
// cross-origin WindowProxy. A null viewer means the current global.
bool WindowView(JSContext* cx, JSObject* viewer, JS::HandleObject target, JS::MutableHandleValue out);
// Host-supplied code (RPC eval, input target expressions, the CLI) is marked
// when compiled. While such a script is the innermost one running, the
// per-access check for the current caller passes for every window, as a
// debugger's would. Views keep their identity: contentWindow is still the
// cross-origin proxy, so page code that is handed it or called from host code
// is checked as itself.
void MarkHostScript(JSScript* script);
bool HostCaller(JSContext* cx);
}  // namespace ruyijs::browser_environment
#endif
