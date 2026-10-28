// Typed data-only resource port; Styling never depends on Network adapters.
#pragma once
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "js/RootingAPI.h"
namespace ruyijs::web_styling {
struct StyleLoadRequest { std::string url, referer, origin; bool cors = false; };
struct StyleLoadResult {
  bool fetched = true;  // the transport delivered a response (asynchronous start port)
  int status = 0;
  std::string body, error;
  std::vector<std::pair<std::string,std::string>> headers;
};
using StyleLoadPort = bool (*)(void*, const StyleLoadRequest&, StyleLoadResult*);
// Starts the request on the asynchronous transport; `done` runs on the JS
// thread when the response (or failure) arrives. False: not started (the
// synchronous port is used instead).
using StyleLoadDone = std::function<void(StyleLoadResult&&)>;
using StyleLoadStartPort = bool (*)(void*, const StyleLoadRequest&, StyleLoadDone done);
using StyleLoadEventPort = bool (*)(void*, JS::HandleObject, const char*);
}
