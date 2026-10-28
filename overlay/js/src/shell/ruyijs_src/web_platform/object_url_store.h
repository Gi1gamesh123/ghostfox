#ifndef RUYIJS_WEB_PLATFORM_OBJECT_URL_STORE_H
#define RUYIJS_WEB_PLATFORM_OBJECT_URL_STORE_H
#include <cstdint>
#include <memory>
#include <string>
#include "js/RootingAPI.h"

namespace ruyijs::web_platform {
// Engine references only: the domain catalog owns validity and accounting.
// A Context shares this adapter; each document has a distinct owner identity.
class ObjectUrlStore final {
 public:
  explicit ObjectUrlStore(JSContext* cx);
  ~ObjectUrlStore();
  uint64_t NewOwner();
  bool Retain(JS::HandleObject object, size_t bytes, const std::string& url,
              const std::string& origin, uint64_t owner, uint64_t principal = 0,
              uint64_t* reference = nullptr);
  bool Contains(uint64_t reference) const;
  bool RetainReference(uint64_t reference, const std::string& url,
                       const std::string& origin, uint64_t owner, uint64_t principal);
  bool Read(const std::string& url, const std::string& origin, uint64_t owner,
            std::string* bytes, std::string* type);
  void Revoke(const std::string& url, const std::string& origin, uint64_t owner);
  void ClearOwner(uint64_t owner);
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}
#endif
