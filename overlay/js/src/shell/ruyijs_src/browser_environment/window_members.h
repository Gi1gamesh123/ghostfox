#ifndef RUYIJS_BROWSER_ENVIRONMENT_WINDOW_MEMBERS_H
#define RUYIJS_BROWSER_ENVIRONMENT_WINDOW_MEMBERS_H
#include "js/CallArgs.h"
#include "js/TypeDecls.h"
namespace ruyijs::browser_environment {
struct State;
// Slot ids for the Window facade members installed on the global after the base
// global accessors (globals.cpp owns slots 0..8). One JSNative per member kind
// dispatches on the slot, so the installer table stays in one place and every
// member keeps exactly one production installer.
enum WindowMember : int32_t {
  // Read-only or replaceable scalar accessors.
  kWindowName = 100, kWindowOrigin, kWindowIsSecureContext, kWindowCrossOriginIsolated,
  kWindowInnerWidth, kWindowInnerHeight, kWindowOuterWidth, kWindowOuterHeight,
  kWindowScreenX, kWindowScreenY, kWindowPageXOffset, kWindowPageYOffset,
  kWindowDevicePixelRatio, kWindowScrollX, kWindowScrollY, kWindowClosed,
  kWindowLength, kWindowFrames, kWindowMozInnerScreenX, kWindowMozInnerScreenY,
  kWindowExternal, kWindowVisualViewport = 122,
  // Methods.
  kWindowAlert = 200, kWindowClose, kWindowOpen, kWindowScrollTo, kWindowScrollBy,
  kWindowScroll, kWindowPostMessage = 207,  // 206 migrated to Document Model Selection
  // WindowOrWorkerGlobalScope encoding and cloning.
  kWindowBtoa = 300, kWindowAtob, kWindowStructuredClone,
  // Window operations the Firefox-feature probes read (T5b second slice). The
  // donor's Function.length is part of each member's shape and travels with the
  // installer table, so no two members share an arity.
  kWindowConfirm = 400, kWindowPrompt, kWindowPrint, kWindowStop, kWindowFocus,
  kWindowBlur, kWindowMoveTo, kWindowMoveBy, kWindowResizeTo, kWindowResizeBy,
  kWindowCaptureEvents, kWindowReleaseEvents, kWindowReportError
};
// Installs every Window facade member the realm does not already have.
bool InstallWindowMembers(State& state);
// Throws through the Document Model's sole DOMException factory. Before the
// runtime binds that port this reports a plain error, which never happens once
// a page can call the members that use it.
bool ThrowDOM(State& state, const char* name, const char* message);
// Install-time member name of one Window facade slot, e.g. kWindowInnerWidth ->
// "innerWidth". screenLeft/screenTop reuse the screenX/screenY slots, so the
// primary name is reported; static names only, never a script-supplied string.
const char* WindowMemberName(int32_t slot);
// The one window scroll operation (window_scroll.cpp): clamps to [0, scroll
// extent], stores the position and, when it moved, queues the scroll/scrollend
// notification. Every Window scrolling member goes through it.
bool ScrollWindowTo(State& state, double x, double y);
// Accessor/setter/method natives shared by the installer above.
bool WindowGetter(JSContext* cx, unsigned argc, JS::Value* vp);
bool WindowSetter(JSContext* cx, unsigned argc, JS::Value* vp);
bool WindowMethod(JSContext* cx, unsigned argc, JS::Value* vp);
bool WindowEncoding(JSContext* cx, unsigned argc, JS::Value* vp);
bool WindowCloning(JSContext* cx, unsigned argc, JS::Value* vp);
bool WindowMessaging(JSContext* cx, unsigned argc, JS::Value* vp);
}
#endif
