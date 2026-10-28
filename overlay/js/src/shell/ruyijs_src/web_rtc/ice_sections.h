#pragma once
#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace ruyijs::web_rtc {
struct IceSection {
  std::string kind, mid;
  int32_t index;
};
inline std::vector<IceSection> IceSections(const std::string& sdp, uint32_t dataPosition,
                                         std::string* ufrag) {
  std::vector<IceSection> sections;
  std::istringstream input(sdp);
  std::string line;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.rfind("m=", 0) == 0) {
      const int32_t index = int32_t(sections.size());
      sections.push_back({line.substr(2, line.find(' ') - 2), std::to_string(index), index});
    } else if (line.rfind("a=mid:", 0) == 0 && !sections.empty()) {
      sections.back().mid = line.substr(6);
    } else if (line.rfind("a=ice-ufrag:", 0) == 0 && ufrag->empty()) {
      *ufrag = line.substr(12);
    }
  }
  const auto data = std::find_if(sections.begin(), sections.end(),
                               [](const auto& section) { return section.kind == "application"; });
  if (data != sections.end() && dataPosition < uint32_t(data - sections.begin()))
    std::rotate(sections.begin() + dataPosition, data, data + 1);
  return sections;
}
}
