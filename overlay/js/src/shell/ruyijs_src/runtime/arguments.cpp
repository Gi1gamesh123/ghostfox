#include "runtime/arguments.h"
#include <cstring>
#include <fstream>
#include <iterator>
#include <cstdlib>
#ifndef _WIN32
#include <unistd.h>
#endif
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace ruyijs {
namespace {
bool ExpandTracePid(std::string& path) {
#ifdef _WIN32
  const std::string pid = std::to_string(GetCurrentProcessId());
#else
  const std::string pid = std::to_string(getpid());
#endif
  bool changed = false;
  size_t offset = 0;
  while ((offset = path.find("{pid}", offset)) != std::string::npos) {
    path.replace(offset, 5, pid);
    offset += pid.size();
    changed = true;
  }
  return changed;
}
// Keep the environment template intact for child processes. The donor's
// existing CLI option takes precedence over its environment default.
void PrepareTracePath(std::vector<std::string>& arguments) {
  for (size_t index = 1; index < arguments.size(); ++index) {
    if (arguments[index] == "--trace-file") {
      if (index + 1 < arguments.size()) ExpandTracePid(arguments[index + 1]);
      return;
    }
    if (arguments[index].rfind("--trace-file=", 0) == 0) {
      ExpandTracePid(arguments[index]);
      return;
    }
  }
  const char* configured = std::getenv("MOZ_JS_STANDALONE_TRACE_FILE");
  if (!configured || !*configured) configured = std::getenv("RUYIJS_TRACE_FILE");
  if (!configured || !*configured) return;
  std::string path(configured);
  if (ExpandTracePid(path)) arguments.push_back("--trace-file=" + path);
}
}  // namespace
#ifdef _WIN32
bool Arguments::Prepare(int* count, char*** values) {
  bool product = false;
  for (int index = 1; index < *count; ++index) {
    if (!strcmp((*values)[index], "--ruyijs-runtime") ||
        !strcmp((*values)[index], "--ruyijs-rpc") ||
        !strncmp((*values)[index], "--ruyijs-rpc=", 13) ||
        !strcmp((*values)[index], "--ruyijs-worker") ||
        !strncmp((*values)[index], "--ruyijs-worker=", 16)) product = true;
  }
  if (!product) return true;
  int wideCount;
  wchar_t** wide = CommandLineToArgvW(GetCommandLineW(), &wideCount);
  if (!wide) return false;
  strings_.reserve(wideCount);
  for (int index = 0; index < wideCount; ++index) {
    int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[index],
                                   -1, nullptr, 0, nullptr, nullptr);
    if (!bytes) { LocalFree(wide); return false; }
    std::string text(bytes, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[index], -1,
                        text.data(), bytes, nullptr, nullptr);
    text.pop_back();
    strings_.push_back(std::move(text));
  }
  LocalFree(wide);
  PrepareTracePath(strings_);
  for (auto& text : strings_) pointers_.push_back(text.data());
  pointers_.push_back(nullptr);
  *count = static_cast<int>(strings_.size());
  *values = pointers_.data();
  return true;
}

bool ReadUTF8File(const char* path, std::string* output) {
  int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, nullptr, 0);
  if (!count) return false;
  std::wstring wide(count, L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide.data(), count);
  std::ifstream file(wide.c_str(), std::ios::binary);
  if (!file) return false;
  output->assign(std::istreambuf_iterator<char>(file), {});
  return !file.bad();
}
#else
// POSIX: argv arrives as UTF-8 already; the trace-path rewrite still runs.
bool Arguments::Prepare(int* count, char*** values) {
  for (int index = 1; index < *count; ++index) {
    if (!strcmp((*values)[index], "--ruyijs-runtime") ||
        !strcmp((*values)[index], "--ruyijs-rpc") ||
        !strncmp((*values)[index], "--ruyijs-rpc=", 13) ||
        !strcmp((*values)[index], "--ruyijs-worker") ||
        !strncmp((*values)[index], "--ruyijs-worker=", 16)) {
      PrepareTracePath(strings_);
      for (auto& text : strings_) pointers_.push_back(text.data());
      pointers_.push_back(nullptr);
      *count = static_cast<int>(strings_.size());
      *values = pointers_.data();
      break;
    }
  }
  return true;
}
bool ReadUTF8File(const char* path, std::string* output) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return false;
  output->assign(std::istreambuf_iterator<char>(file), {});
  return !file.bad();
}
#endif
}  // namespace ruyijs
