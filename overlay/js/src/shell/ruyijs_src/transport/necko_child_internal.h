#pragma once
#include "transport/necko_child.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace ruyijs::transport::child_detail {
inline JobStatus Error(const char* operation) noexcept {
  const auto code = GetLastError();
  return {code ? code : ERROR_GEN_FAILURE, operation};
}
JobStatus Prepare(const std::wstring& executable, const std::wstring& cwd,
                  const std::vector<std::wstring>& args,
                  std::wstring* command, std::vector<wchar_t>* environment);
}
