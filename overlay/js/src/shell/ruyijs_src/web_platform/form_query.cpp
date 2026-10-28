// Original form-urlencoded query parser at its native allocation boundary.
#include "web_platform/form_query.h"
#include <string_view>

namespace ruyijs::web_platform {
namespace {
int Digit(char unit) {
  if (unit >= '0' && unit <= '9') return unit - '0';
  if (unit >= 'a' && unit <= 'f') return unit - 'a' + 10;
  if (unit >= 'A' && unit <= 'F') return unit - 'A' + 10;
  return -1;
}
std::string FormDecode(std::string_view text) {
  std::string out;
  for (size_t index = 0; index < text.size(); ++index) {
    char unit = text[index];
    if (unit == '+') { out.push_back(' '); continue; }
    if (unit == '%' && index + 2 < text.size()) {
      int high = Digit(text[index + 1]), low = Digit(text[index + 2]);
      if (high >= 0 && low >= 0) {
        out.push_back(char((high << 4) | low));
        index += 2;
        continue;
      }
    }
    out.push_back(unit);
  }
  return out;
}
}
bool ParseFormQuery(const std::string& query,
                    std::vector<std::pair<std::string,std::string>>* entries) noexcept {
  const size_t original=entries->size();
  try {
    size_t begin = 0;
    while (begin <= query.size() && !query.empty()) {
      size_t stop = query.find('&', begin);
      std::string part = query.substr(begin, stop == std::string::npos ? std::string::npos
                                                                      : stop - begin);
      if (!part.empty()) {
        size_t equals = part.find('=');
        std::string name = equals == std::string::npos ? part : part.substr(0, equals);
        std::string value = equals == std::string::npos ? "" : part.substr(equals + 1);
        entries->emplace_back(FormDecode(name), FormDecode(value));
      }
      if (stop == std::string::npos) break;
      begin = stop + 1;
    }
    return true;
  } catch (...) {
    entries->erase(entries->begin()+original,entries->end());
    return false;
  }
}
}
