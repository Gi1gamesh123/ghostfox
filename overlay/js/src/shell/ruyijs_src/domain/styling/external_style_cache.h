// CSS loader's bounded immutable-source cache. No DOM, rule or JS references.
#pragma once
#include <memory>
#include <string>
#include <vector>
namespace ruyijs::domain::styling {
struct ExternalStyleResource {
  std::string url, documentURL, origin, mode, text;
  bool success = false, originClean = true;
};
class ExternalStyleCache final {
 public:
  ExternalStyleCache(size_t entries, size_t bytes) : entryLimit_(entries), byteLimit_(bytes) {}
  std::shared_ptr<const ExternalStyleResource> Find(const std::string&, const std::string&,
                                                   const std::string&, const std::string&);
  void Put(std::shared_ptr<const ExternalStyleResource>);
  size_t bytes() const { return bytes_; }
  size_t size() const { return values_.size(); }
 private:
  size_t entryLimit_, byteLimit_, bytes_ = 0;
  std::vector<std::shared_ptr<const ExternalStyleResource>> values_;
};
}
