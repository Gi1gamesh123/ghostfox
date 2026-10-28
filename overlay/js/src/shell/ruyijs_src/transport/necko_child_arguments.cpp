#include "transport/necko_child_internal.h"
#include <cwchar>
#include <string_view>

namespace ruyijs::transport::child_detail {
namespace {
constexpr size_t kMaxCharacters = 32767;
bool Absolute(const std::wstring& path) {
  if (path.empty() || path.size() >= kMaxCharacters || path.find(L'\0') != path.npos)
    return false;
  const auto slash = [](wchar_t ch) { return ch == L'\\' || ch == L'/'; };
  if (path.size() >= 3 && ((path[0] >= L'A' && path[0] <= L'Z') ||
      (path[0] >= L'a' && path[0] <= L'z')) && path[1] == L':' && slash(path[2]))
    return true;
  if (path.size() < 5 || !slash(path[0]) || !slash(path[1])) return false;
  const auto separator = path.find_first_of(L"\\/", 2);
  return separator != path.npos && separator > 2 && separator + 1 < path.size();
}
void Quote(const std::wstring& input, std::wstring* output) {
  output->push_back(L'"');
  size_t slashes = 0;
  for (const auto ch : input) {
    if (ch == L'\\') { ++slashes; continue; }
    output->append(slashes * (ch == L'"' ? 2 : 1), L'\\');
    slashes = 0;
    if (ch == L'"') output->push_back(L'\\');
    output->push_back(ch);
  }
  output->append(slashes * 2, L'\\');
  output->push_back(L'"');
}
bool Excluded(std::wstring_view entry) {
  const auto separator = entry.find(L'=');
  if (separator == entry.npos || separator == 0) return false;
  constexpr const wchar_t* names[] = {L"PYTHONPATH", L"PYTHONHOME", L"PYTHONSTARTUP",
      L"PYTHONUSERBASE", L"VIRTUAL_ENV", L"NECKO_CORE_LIBRARY", L"NECKO_QUIC_LIBRARY"};
  for (const auto name : names)
    if (CompareStringOrdinal(entry.data(), static_cast<int>(separator), name, -1, TRUE) == CSTR_EQUAL)
      return true;
  return false;
}
JobStatus Environment(std::vector<wchar_t>* output) {
  struct EnvironmentOwner {
    wchar_t* value = GetEnvironmentStringsW();
    ~EnvironmentOwner() { if (value) FreeEnvironmentStringsW(value); }
  } source;
  if (!source.value) return Error("GetEnvironmentStringsW");
  for (auto cursor = source.value; *cursor;) {
    const size_t size = wcslen(cursor);
    if (size >= kMaxCharacters) return {ERROR_BUFFER_OVERFLOW, "EnvironmentBound"};
    if (!Excluded({cursor, size})) {
      if (output->size() + size + 2 > kMaxCharacters)
        return {ERROR_BUFFER_OVERFLOW, "EnvironmentBound"};
      output->insert(output->end(), cursor, cursor + size + 1);
    }
    cursor += size + 1;
  }
  if (output->empty()) output->push_back(L'\0');
  output->push_back(L'\0');
  return {};
}
}
JobStatus Prepare(const std::wstring& executable, const std::wstring& cwd,
                  const std::vector<std::wstring>& args,
                  std::wstring* command, std::vector<wchar_t>* environment) {
  if (!Absolute(executable) || !Absolute(cwd) || executable.find(L'"') != executable.npos)
    return {ERROR_INVALID_PARAMETER, "AbsolutePaths"};
  Quote(executable, command);
  for (const auto& argument : args) {
    if (argument.size() >= kMaxCharacters || argument.find(L'\0') != argument.npos)
      return {ERROR_INVALID_PARAMETER, "Argument"};
    if (command->size() + argument.size() + 4 > kMaxCharacters)
      return {ERROR_BUFFER_OVERFLOW, "CommandBound"};
    command->push_back(L' ');
    Quote(argument, command);
    if (command->size() + 1 > kMaxCharacters) return {ERROR_BUFFER_OVERFLOW, "CommandBound"};
  }
  if (command->size() + 1 > kMaxCharacters) return {ERROR_BUFFER_OVERFLOW, "CommandBound"};
  return Environment(environment);
}
}
