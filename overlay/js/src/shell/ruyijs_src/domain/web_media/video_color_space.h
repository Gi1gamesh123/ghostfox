#pragma once
#include <string_view>

namespace ruyijs::domain::web_media {
// WebCodecs VideoColorSpaceInit enumeration values, indexed by its IDL fields.
inline bool ValidColorSpaceValue(unsigned field, std::string_view value) {
  if (field == 0)
    return value == "bt709" || value == "bt470bg" || value == "smpte170m" ||
           value == "bt2020" || value == "smpte432";
  if (field == 1)
    return value == "bt709" || value == "smpte170m" || value == "iec61966-2-1" ||
           value == "linear" || value == "pq" || value == "hlg";
  if (field == 2)
    return value == "rgb" || value == "bt709" || value == "bt470bg" ||
           value == "smpte170m" || value == "bt2020-ncl";
  return false;
}
}
