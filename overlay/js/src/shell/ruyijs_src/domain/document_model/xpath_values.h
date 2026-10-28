#pragma once
#include <cstdint>

namespace ruyijs::domain::document_model {
// Public empty-result policy. There is no XPath parser or node result cache.
constexpr uint16_t xpath_result_type(uint16_t type) { return type ? type : 2; }
constexpr bool xpath_iterator(uint16_t type) { return type == 4 || type == 5; }
constexpr bool xpath_snapshot(uint16_t type) { return type == 6 || type == 7; }
constexpr bool xpath_single_node(uint16_t type) { return type == 8 || type == 9; }
}
