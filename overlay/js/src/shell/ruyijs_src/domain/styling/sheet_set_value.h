// Authored Styling value: the document's first non-empty preferred title.
#pragma once
#include <string>

namespace ruyijs::domain::styling {
class SheetSetValue final {
 public:
  void observe(const std::string& title) {
    if (preferred_.empty() && !title.empty()) preferred_ = title;
  }
  bool disabled(const std::string& title) const {
    return !title.empty() && title != preferred_;
  }
 private:
  std::string preferred_;
};
}
