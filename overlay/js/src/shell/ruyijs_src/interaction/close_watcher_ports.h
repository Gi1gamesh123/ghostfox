#pragma once
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs::interaction {
// Borrowed original-owner capabilities; no copied context state. The abort
// capability is the realm's AbortPorts (abort_ports.h).
struct CloseWatcherPorts {
  bool (*active)(void*) = nullptr;
  bool (*history)(void*) = nullptr;
  void (*consume)(void*) = nullptr;
  bool (*input)(void*,JS::HandleObject) = nullptr;
};
}
