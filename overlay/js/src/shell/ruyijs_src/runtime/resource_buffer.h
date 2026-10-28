#ifndef RUYIJS_RESOURCE_BUFFER_H
#define RUYIJS_RESOURCE_BUFFER_H
#include "resource_network/model.h"

namespace ruyijs::resource_wire {
using resource_network::Status;
Status Allocate(std::string* output, size_t bytes) noexcept;
Status Append(resource_network::HeaderPairs* output, std::string name, std::string value) noexcept;
Status DecodeBase64(std::string_view encoded, std::string* output) noexcept;
Status EncodeBase64(std::string_view bytes, std::string* output) noexcept;
bool ValidUTF8(std::string_view bytes) noexcept;
}
#endif
