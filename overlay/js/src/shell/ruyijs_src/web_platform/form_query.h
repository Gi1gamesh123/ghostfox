#pragma once
#include <string>
#include <utility>
#include <vector>

namespace ruyijs::web_platform {
// Original query parser; allocation failure never crosses the no-EH JS bridge.
bool ParseFormQuery(const std::string&,
                    std::vector<std::pair<std::string,std::string>>*) noexcept;
}
