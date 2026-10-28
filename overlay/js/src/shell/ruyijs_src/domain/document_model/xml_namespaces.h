#pragma once
#include "domain/document_model/node_type.h"
#include <vector>

namespace ruyijs::domain::document_model {
inline constexpr DomStringView kXmlNamespace = u"http://www.w3.org/XML/1998/namespace";
inline constexpr DomStringView kXmlnsNamespace = u"http://www.w3.org/2000/xmlns/";
inline DomString xml_qualified(DomStringView prefix, DomStringView local) {
  return prefix.empty() ? DomString(local) : DomString(prefix) + u":" + DomString(local);
}
// One traversal stack, restored on element exit. No per-node namespace cache.
class XmlNamespaces {
 public:
  struct Binding { DomString prefix, uri; };
  size_t size() const noexcept { return bindings_.size(); }
  void restore(size_t size) { bindings_.resize(size); }
  void push(DomStringView prefix, DomStringView uri) {
    bindings_.push_back({DomString(prefix), DomString(uri)});
  }
  bool confirm(DomString& prefix, DomStringView uri, size_t scope, bool attribute) {
    if (prefix == u"xmlns") return false;
    if (uri == kXmlNamespace) { prefix = u"xml"; return false; }
    if (attribute && uri.empty()) { prefix.clear(); return false; }
    for (;;) {
      bool seen = false, matched = false, restart = false;
      DomString closest;
      for (size_t index = bindings_.size(); index-- > 0;) {
        const auto& binding = bindings_[index];
        if (prefix == binding.prefix) {
          if (!seen && uri == binding.uri) { matched = true; closest = prefix; break; }
          seen = true;
          if (!prefix.empty() || index >= scope) {
            prefix = generated(); restart = true; break;
          }
        }
        if (!matched && uri == binding.uri) {
          bool visible = true;
          for (size_t later = index + 1; later < bindings_.size(); ++later)
            if (bindings_[later].prefix == binding.prefix) { visible = false; break; }
          if (visible) { matched = true; closest = binding.prefix; }
        }
      }
      if (restart) continue;
      if (matched && (!attribute || !closest.empty())) { prefix = closest; return false; }
      if (prefix.empty()) {
        if (attribute) { prefix = generated(); continue; }
        if (!seen && uri.empty()) return false;
      }
      return true;
    }
  }
 private:
  DomString generated() { return u"a" + from_utf8(std::to_string(sequence_++)); }
  std::vector<Binding> bindings_;
  size_t sequence_ = 0;
};
}
