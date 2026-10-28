#pragma once
#include "resource_network/model.h"

namespace ruyijs::resource_network {
Status AppendBodyBytes(std::string*,const uint8_t*,size_t) noexcept;
Status CopyBodyBytes(const std::string&,std::string*) noexcept;
}
