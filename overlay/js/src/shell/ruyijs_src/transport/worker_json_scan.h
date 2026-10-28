#pragma once
#include "transport/worker_json.h"
namespace ruyijs::transport::worker {
Status ValidateUtf8(std::string_view bytes) noexcept;
Status ScanJsonQuota(std::string_view json) noexcept;
}
