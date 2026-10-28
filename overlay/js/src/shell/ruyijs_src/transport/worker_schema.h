#pragma once
#include "transport/worker_json.h"

namespace ruyijs::transport::worker {
enum class Sender { Parent, Worker };
// Validates the W1 payload and direction in the existing immutable JSON tree.
// Identity continuity, channel ordering, deadlines and credit belong to owners.
Status ValidatePayload(const ControlDocument& document, Sender sender) noexcept;
}
