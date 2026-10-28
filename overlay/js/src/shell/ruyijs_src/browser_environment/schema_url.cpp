#include "browser_environment/schema.h"

namespace ruyijs::browser_environment {
bool ValidURL(JSContext* cx, const std::string& value) {
  auto fail = [cx]() { return Error(cx,
      "Location profile requires a canonical HTTP(S) absolute URL", JSEXN_RANGEERR); };
  size_t begin = value.rfind("https://", 0) == 0 ? 8 :
                 value.rfind("http://", 0) == 0 ? 7 : 0;
  if (!begin || value.size() > 16384) return fail();
  for (unsigned char unit : value) {
    bool alpha = (unit >= 'a' && unit <= 'z') || (unit >= 'A' && unit <= 'Z');
    bool digit = unit >= '0' && unit <= '9';
    if (!alpha && !digit && std::string("-._~:/?#[]@!$&()*+,;=%").find(unit) ==
                              std::string::npos) return fail();
  }
  size_t end = value.find_first_of("/?#", begin);
  if (end == std::string::npos || value[end] != '/' || end == begin) return fail();
  std::string authority = value.substr(begin, end - begin);
  size_t colon = authority.find(':');
  std::string host = authority.substr(0, colon);
  if (host.empty() || host.front() == '.' || host.back() == '.' ||
      host.find("..") != std::string::npos) return fail();
  for (unsigned char unit : host)
    if (!(unit >= 'a' && unit <= 'z') && !(unit >= '0' && unit <= '9') &&
        unit != '.' && unit != '-') return fail();
  std::string last = host.substr(host.find_last_of('.') == std::string::npos
      ? 0 : host.find_last_of('.') + 1);
  bool numeric = !last.empty();
  for (char unit : last) numeric = numeric && unit >= '0' && unit <= '9';
  if (last.rfind("0x", 0) == 0) return fail();
  if (numeric) {
    size_t offset = 0;
    for (unsigned part = 0; part < 4; ++part) {
      size_t stop = host.find('.', offset);
      if ((part < 3) != (stop != std::string::npos)) return fail();
      std::string digits = host.substr(offset, stop - offset);
      if (digits.empty() || digits.size() > 3 ||
          (digits.size() > 1 && digits[0] == '0')) return fail();
      unsigned number = 0;
      for (char digit : digits) {
        if (digit < '0' || digit > '9') return fail();
        number = number * 10 + digit - '0';
      }
      if (number > 255) return fail();
      offset = stop + 1;
    }
  }
  if (colon != std::string::npos) {
    std::string port = authority.substr(colon + 1);
    if (port.empty() || port.size() > 5 || (port.size() > 1 && port[0] == '0')) return fail();
    unsigned number = 0;
    for (unsigned char unit : port) {
      if (unit < '0' || unit > '9') return fail();
      number = number * 10 + unit - '0';
    }
    if (number > 65535 || (begin == 8 && number == 443) ||
        (begin == 7 && number == 80)) return fail();
  }
  std::string path = value.substr(end, value.find_first_of("?#", end) - end);
  if (value.back() == '?' || value.back() == '#' || value.find("?#") != std::string::npos)
    return fail();
  if (path.find("/./") != std::string::npos || path.find("/../") != std::string::npos ||
      path.ends_with("/.") || path.ends_with("/..")) return fail();
  for (size_t index = 0; index < path.size(); ++index) {
    if (path[index] != '%') continue;
    if (index + 2 >= path.size()) return fail();
    auto hex = [](char c) { return (c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); };
    if (!hex(path[index + 1]) || !hex(path[index + 2])) return fail();
    if (path[index + 1] == '2' && (path[index + 2] == 'e' || path[index + 2] == 'E'))
      return fail();
    index += 2;
  }
  return true;
}
}
