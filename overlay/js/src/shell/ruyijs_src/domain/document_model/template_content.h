#pragma once
#include <cstdint>

namespace ruyijs::domain::document_model {
// Reference into the composition's document identity spaces. The existing
// DocumentAggregates own the referenced fragment; this is never another tree.
struct TemplateContentRef {
  uint64_t document = 0;
  uint64_t node = 0;
  bool operator==(const TemplateContentRef& other) const noexcept {
    return document == other.document && node == other.node;
  }
};
}
