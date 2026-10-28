// body/frameset window-reflected handler names. These are the content
// attributes and GlobalEventHandlers IDL members Firefox forwards from
// HTMLBodyElement/HTMLFrameSetElement to their Window
// (nsGenericHTMLElement::GetEventListenerManagerForAttr and
// nsGenericHTMLElement::GetOn*/SetOn*, WINDOW_EVENT / FORWARDED_EVENT in
// EventNameList.inc). Metadata only: the Window accessor owner
// (browser_environment) stays the single handler store, and the Document Model
// routes these names to it without a second handler layer.
#ifndef RUYIJS_DOCUMENT_MODEL_BODY_FRAMESET_HANDLERS_H
#define RUYIJS_DOCUMENT_MODEL_BODY_FRAMESET_HANDLERS_H
#include <cstddef>
#include <cstring>
namespace ruyijs::document_model {
// WindowEventHandlers mixin members, then the GlobalEventHandlers members that
// body/frameset reflect to the Window (onpagereveal is pref-disabled).
inline constexpr const char* kBodyFramesetWindowHandlerNames[] = {
  "onafterprint", "onbeforeprint", "onbeforeunload", "onhashchange",
  "onlanguagechange", "onmessage", "onmessageerror", "onoffline", "ononline",
  "onpagehide", "onpageshow", "onpopstate", "onrejectionhandled", "onstorage",
  "onunhandledrejection", "onunload", "ongamepadconnected", "ongamepaddisconnected",
  "onblur", "onerror", "onfocus", "onload", "onresize", "onscroll",
};
inline constexpr size_t kBodyFramesetWindowHandlerCount = 24;
inline bool IsBodyFramesetInterface(const char* interface_name) {
  return !std::strcmp(interface_name, "HTMLBodyElement") ||
         !std::strcmp(interface_name, "HTMLFrameSetElement");
}
inline bool IsBodyFramesetWindowHandler(const char* name) {
  for (size_t i = 0; i < kBodyFramesetWindowHandlerCount; ++i)
    if (!std::strcmp(kBodyFramesetWindowHandlerNames[i], name)) return true;
  return false;
}
}
#endif
