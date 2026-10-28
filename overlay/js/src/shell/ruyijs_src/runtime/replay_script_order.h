#ifndef RUYIJS_REPLAY_SCRIPT_ORDER_H
#define RUYIJS_REPLAY_SCRIPT_ORDER_H
#include "js/RootingAPI.h"
#include <string>
#include <vector>

namespace ruyijs {
struct ReplayScriptOrder {
  std::vector<std::string> urls;
  bool Contains(const std::string& url) const;
};
bool ReadReplayScriptOrder(JSContext*, JS::HandleObject, ReplayScriptOrder*);
}
#endif
