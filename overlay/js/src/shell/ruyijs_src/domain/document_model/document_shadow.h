#pragma once
#include "domain/document_model/document.h"

namespace ruyijs::domain::document_model {
inline bool shadow_host_name(DomStringView name) {
  static const char16_t* allowed[] = {u"article",u"aside",u"blockquote",u"body",u"div",
    u"footer",u"h1",u"h2",u"h3",u"h4",u"h5",u"h6",u"header",u"main",u"nav",u"p",u"section",u"span"};
  for (auto* value : allowed) if (name == value) return true;
  if (name.empty() || name[0] < u'a' || name[0] > u'z' || name.find(u'-') == DomStringView::npos) return false;
  static const char16_t* reserved[] = {u"annotation-xml",u"color-profile",u"font-face",u"font-face-src",
    u"font-face-uri",u"font-face-format",u"font-face-name",u"missing-glyph"};
  for (auto* value : reserved) if (name == value) return false;
  for (size_t i = 0; i < name.size(); ++i) {
    uint32_t c = name[i];
    if (c >= 0xd800 && c <= 0xdbff && i + 1 < name.size() && name[i+1] >= 0xdc00 && name[i+1] <= 0xdfff)
      c = 0x10000 + ((c - 0xd800) << 10) + (name[++i] - 0xdc00);
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' ||
        c == 0xb7 || (c >= 0xc0 && c <= 0xd6) || (c >= 0xd8 && c <= 0xf6) ||
        (c >= 0xf8 && c <= 0x37d) || (c >= 0x37f && c <= 0x1fff) ||
        (c >= 0x200c && c <= 0x200d) || (c >= 0x203f && c <= 0x2040) ||
        (c >= 0x2070 && c <= 0x218f) || (c >= 0x2c00 && c <= 0x2fef) ||
        (c >= 0x3001 && c <= 0xd7ff) || (c >= 0xf900 && c <= 0xfdcf) ||
        (c >= 0xfdf0 && c <= 0xfffd) || (c >= 0x10000 && c <= 0xeffff)) continue;
    return false;
  }
  return true;
}
inline NodeId DocumentAggregate::attach_shadow_root(NodeId host, ShadowOptions options) {
  auto* element = get_element(host);
  if (!element || !element->tag_name().html() || !shadow_host_name(element->tag_name().local16()))
    throw DomError(DomErrorCode::NotSupported, "Element does not support a shadow root");
  if (element->shadow_root_id()) throw DomError(DomErrorCode::NotSupported, "Element already has a shadow root");
  NodeId key = next_id();
  nodes_.emplace(key, std::make_unique<ShadowRootNode>(key, host, options));
  element->set_shadow_root(key);
  return key;
}
inline NodeId DocumentAggregate::composed_root_id(NodeId key) const {
  for (;;) {
    key = root_id(key);
    NodeId host = require(key).shadow_host_id();
    if (!host) return key;
    key = host;
  }
}
inline std::vector<NodeId> DocumentAggregate::owned_descendants(NodeId root, bool include_root) const {
  std::vector<NodeId> result, pending{root};
  while (!pending.empty()) {
    NodeId key = pending.back(); pending.pop_back();
    const auto& node = require(key);
    if (include_root || key != root) result.push_back(key);
    if (node.shadow_root_id()) pending.push_back(node.shadow_root_id());
    for (auto it = node.child_ids().rbegin(); it != node.child_ids().rend(); ++it) pending.push_back(*it);
  }
  return result;
}
}
