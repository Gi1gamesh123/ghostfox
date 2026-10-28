#pragma once
#include "transport/worker_json.h"
#include <json/value.h>

namespace ruyijs::transport::worker {
// Infrastructure-private read access. The document owns this immutable tree.
struct ControlAccess {
  static const Json::Value* Root(const ControlDocument& document) noexcept;
};
}
