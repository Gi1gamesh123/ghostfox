#ifndef RUYIJS_BROWSER_ENVIRONMENT_WINDOW_HANDLERS_H
#define RUYIJS_BROWSER_ENVIRONMENT_WINDOW_HANDLERS_H
#include "browser_environment/browser_chrome.h"
#include "browser_environment/window_handler_names.h"
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::browser_environment {
enum HandlerStoreSlot : unsigned {
  kHandlerValues, kHandlerCallbacks, kHandlerAdd, kHandlerRemove, kHandlerPrevent,
  kHandlerErrorMessage, kHandlerErrorFilename, kHandlerErrorLineno,
  kHandlerErrorColno, kHandlerErrorValue, kHandlerBeforeUnloadGet, kHandlerBeforeUnloadSet,
  kHandlerStoreSlots
};
bool InstallWindowHandlers(State&, interaction::Realm&);
bool InvokeWindowHandler(JSContext*, unsigned, JS::Value*);
bool WindowHandlerResult(JSContext*, JS::HandleObject store, unsigned index,
                         JS::HandleObject event, JS::HandleValue function,
                         JS::HandleObject window);
}
#endif
