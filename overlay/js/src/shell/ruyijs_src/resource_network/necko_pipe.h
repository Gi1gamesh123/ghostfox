#ifndef RUYIJS_NECKO_PIPE_H
#define RUYIJS_NECKO_PIPE_H
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
namespace ruyijs::resource_network {
// One owned OS endpoint (pipe handle / descriptor) with the same shape on
// every platform: POSIX keeps the fd in the pointer-sized slot.
class NeckoHandle {
 public:
  explicit NeckoHandle(HANDLE value = nullptr) : value_(value) {}
  ~NeckoHandle() { Reset(); }
  NeckoHandle(const NeckoHandle&) = delete;
  NeckoHandle& operator=(const NeckoHandle&) = delete;
  HANDLE Get() const { return value_; }
  explicit operator bool() const { return value_ && value_ != INVALID_HANDLE_VALUE; }
  void Reset(HANDLE value = nullptr) {
    if (*this) CloseHandle(value_);
    value_ = value;
  }
 private:
  HANDLE value_;
};
#else
#include <unistd.h>
namespace ruyijs::resource_network {
using DWORD = unsigned long;
using ULONGLONG = unsigned long long;
using BOOL = int;
constexpr int TRUE_ = 1;
class NeckoHandle {
 public:
  // The empty sentinel is -1: nullptr would read as fd 0 (stdin) through
  // Get(), and every destructor Reset() of an unopened handle would close it.
  NeckoHandle() : value_(reinterpret_cast<void*>(-1)) {}
  explicit NeckoHandle(void* value) : value_(value) {}
  ~NeckoHandle() { Reset(); }
  NeckoHandle(const NeckoHandle&) = delete;
  NeckoHandle& operator=(const NeckoHandle&) = delete;
  int Get() const { return static_cast<int>(reinterpret_cast<intptr_t>(value_)); }
  explicit operator bool() const { return Get() >= 0; }
  void Reset(void* value = reinterpret_cast<void*>(-1)) {
    if (*this) close(Get());
    value_ = value;
  }
 private:
  void* value_ = reinterpret_cast<void*>(-1);
};
#endif

DWORD NeckoRemaining(ULONGLONG deadline);
class NeckoPipe {
 public:
  bool Open(bool parentWrites);
#ifdef _WIN32
  HANDLE Child() const { return child_.Get(); }
#else
  int Child() const { return child_.Get(); }
#endif
  void CloseChild() { child_.Reset(); }
  void CloseParent() { parent_.Reset(); }
  bool Write(const void* data, uint32_t size, ULONGLONG deadline);
  bool Read(void* data, uint32_t size, ULONGLONG deadline);
 private:
  bool Transfer(void* data, uint32_t size, bool writing, ULONGLONG deadline);
  NeckoHandle parent_, child_, event_;
};
}
#endif
