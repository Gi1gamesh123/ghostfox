#ifndef RUYIJS_WEB_WORKERS_BOOTSTRAP_H
#define RUYIJS_WEB_WORKERS_BOOTSTRAP_H
#include <string>
#include <string_view>
#include "runtime/pipe.h"
namespace ruyijs::web_workers {
inline constexpr size_t kStartupEnvironmentLimit = 1024 * 1024;
// The lifecycle adapter transports this frame without interpreting its payload.
inline bool EncodeStartup(std::string_view url, std::string_view script, std::string* frame,
                          std::u16string_view name = {}, std::string_view environment = {}) {
  if (url.empty() || url.size() > 4096 || url.find('\0') != std::string_view::npos ||
      name.size() > 65536 || environment.size() > kStartupEnvironmentLimit ||
      script.size() > Pipe::kMaxFrame - 16 - url.size() - name.size() * 2 - environment.size()) return false;
  frame->assign("RW4\0", 4);
  for (unsigned index = 0; index < 4; ++index) frame->push_back(char(url.size() >> (8 * index)));
  for (unsigned index = 0; index < 4; ++index) frame->push_back(char(name.size() >> (8 * index)));
  for (unsigned index = 0; index < 4; ++index) frame->push_back(char(environment.size() >> (8 * index)));
  frame->append(url);
  for (char16_t unit : name) {
    frame->push_back(char(unit & 255)); frame->push_back(char(unit >> 8));
  }
  frame->append(environment);
  frame->append(script);
  return true;
}
inline bool DecodeStartup(std::string_view frame, std::string* url, std::u16string* name,
                          std::string_view* script, std::string_view* environment) {
  if (frame.size() < 16 || frame.size() > Pipe::kMaxFrame || frame.substr(0, 4) != std::string_view("RW4\0", 4))
    return false;
  uint32_t length = 0, units = 0, environmentLength = 0;
  for (unsigned index = 0; index < 4; ++index) {
    length |= uint32_t(static_cast<unsigned char>(frame[index + 4])) << (8 * index);
    units |= uint32_t(static_cast<unsigned char>(frame[index + 8])) << (8 * index);
    environmentLength |= uint32_t(static_cast<unsigned char>(frame[index + 12])) << (8 * index);
  }
  if (!length || length > 4096 || units > 65536 || environmentLength > kStartupEnvironmentLimit ||
      size_t(length) + size_t(units) * 2 + environmentLength > frame.size() - 16) return false;
  const auto address = frame.substr(16, length);
  if (address.find('\0') != std::string_view::npos) return false;
  url->assign(address); name->clear(); name->reserve(units);
  for (uint32_t index = 0; index < units; ++index) {
    const size_t offset = 16 + length + size_t(index) * 2;
    name->push_back(char16_t(static_cast<unsigned char>(frame[offset]) |
                            (uint16_t(static_cast<unsigned char>(frame[offset + 1])) << 8)));
  }
  const size_t environmentOffset = 16 + length + size_t(units) * 2;
  *environment = frame.substr(environmentOffset, environmentLength);
  size_t scriptOffset = environmentOffset + environmentLength;
  *script = frame.substr(scriptOffset);
  return true;
}
}
#endif
