#pragma once
#include "js/RootingAPI.h"
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace ruyijs::web_platform {
struct CompressionFree {void operator()(uint8_t* value) const {std::free(value);}};
using CompressionBytes=std::unique_ptr<uint8_t,CompressionFree>;
bool CompressionInput(JSContext*,JS::HandleValue,CompressionBytes*,size_t*);
}
