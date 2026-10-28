#pragma once
#include "domain/browser_environment/url_value.h"
#include <cstdint>
#include <string_view>

namespace ruyijs::domain::browser_environment {
struct WorkerContext {
  OriginValue origin;
  bool secure = false, isolated = false;
};
inline bool LoopbackIpv4(std::string_view host) {
  uint64_t address = 0;
  unsigned count = 0;
  while (!host.empty()) {
    const size_t dot = host.find('.');
    auto part = host.substr(0, dot);
    if (part.empty() || ++count > 4) return false;
    unsigned base = 10;
    if (part.size() > 1 && part[0] == '0') {
      base = 8; part.remove_prefix(1);
      if (!part.empty() && part[0] == 'x') { base = 16; part.remove_prefix(1); }
    }
    uint64_t number = 0;
    for (char c : part) {
      const unsigned digit = c >= '0' && c <= '9' ? unsigned(c - '0') :
                             c >= 'a' && c <= 'f' ? unsigned(c - 'a' + 10) : 16;
      if (digit >= base || number > (UINT32_MAX - digit) / base) return false;
      number = number * base + digit;
    }
    if (dot == std::string_view::npos) {
      const unsigned bits = 8 * (5 - count);
      if (number >= (uint64_t(1) << bits)) return false;
      return ((address << bits | number) >> 24) == 127;
    }
    if (number > 255) return false;
    address = (address << 8) | number;
    host.remove_prefix(dot + 1);
  }
  return false;
}
inline bool LoopbackIpv6(std::string_view host) {
  if (host.size() < 4 || host.front() != '[' || host.back() != ']') return false;
  host.remove_prefix(1); host.remove_suffix(1);
  if (host.find(":::") != std::string_view::npos) return false;
  const size_t gap = host.find("::");
  if (gap != std::string_view::npos && host.find("::", gap + 2) != std::string_view::npos) return false;
  unsigned count = 0, last = 0;
  bool foundOne = false;
  while (!host.empty()) {
    const size_t colon = host.find(':');
    const auto group = host.substr(0, colon);
    if (!group.empty()) {
      if (foundOne || group.size() > 4 || ++count > 8) return false;
      unsigned value = 0;
      for (char c : group) {
        if (c < '0' || c > '1') return false;
        value = value * 16 + unsigned(c - '0');
      }
      if (value > 1) return false;
      last = value; foundOne = value == 1;
    } else if (gap == std::string_view::npos) return false;
    if (colon == std::string_view::npos) break;
    if (foundOne) return false;
    host.remove_prefix(colon + 1);
  }
  return last == 1 && (gap == std::string_view::npos ? count == 8 : count < 8);
}
inline bool TrustworthyContextUrl(const UrlValue& url) {
  if (url.protocol() == "https:" || url.protocol() == "wss:" || url.protocol() == "file:") return true;
  if (url.protocol() == "blob:") return TrustworthyContextUrl(UrlValue(url.origin()));
  if (url.protocol() != "http:" && url.protocol() != "ws:") return false;
  std::string_view host = url.hostname();
  if (host.ends_with('.')) host.remove_suffix(1);
  return host == "localhost" || host.ends_with(".localhost") || LoopbackIpv4(host) || LoopbackIpv6(host);
}
}
