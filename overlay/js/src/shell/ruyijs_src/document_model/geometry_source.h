#pragma once
#include "document_model/model.h"

namespace ruyijs::document_model {
struct GeometryNode {
  bool element = false, graphics = false, textContent = false, connected = false;
  DomString localName, text;
  std::vector<AttributeSnapshot> attributes;
  void* factory = nullptr;  // Non-owning service of the original Wrapper.factory realm.
};
}
