#ifndef RUYIJS_BROWSER_ENVIRONMENT_WINDOW_LEGACY_H
#define RUYIJS_BROWSER_ENVIRONMENT_WINDOW_LEGACY_H
// Firefox Window members outside the WHATWG Window facade (status, event,
// clientInformation, scrollMaxX/Y, scrollByLines/Pages, fullScreen,
// InstallTrigger, originAgentCluster, updateCommands, find, dump, setResizable)
// and documentPictureInPicture (docs/owners/BROWSER_ENVIRONMENT.md, 2026-10-21).
#include <string>
#include "browser_environment/state.h"
#include "runtime/web_kit.h"

namespace ruyijs::interaction { class Realm; }
namespace ruyijs::browser_environment {
struct WindowLegacyState {
  WindowLegacyState(State& env, interaction::Realm& events);
  State& env;
  interaction::Realm& events;
  WebKit kit;
  WebFace pip;
  JS::PersistentRootedObject pipInstance;
  JS::PersistentRootedValue onenter;
  std::string status;
};
bool InstallWindowLegacy(State& state, interaction::Realm& events);
// document_picture_in_picture.cpp: the interface and the window's one instance.
bool InstallDocumentPictureInPicture(WindowLegacyState& legacy);
// PictureInPictureWindow, interface object only (presentation_views.cpp).
bool InstallPictureInPictureWindow(WindowLegacyState& legacy);
}  // namespace ruyijs::browser_environment
#endif
