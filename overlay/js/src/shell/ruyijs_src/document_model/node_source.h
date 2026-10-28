// Temporary identity resolved from the original native Node receiver.
#ifndef RUYIJS_DOCUMENT_NODE_SOURCE_H
#define RUYIJS_DOCUMENT_NODE_SOURCE_H
#include "document_model/model.h"

namespace ruyijs::document_model {
class Realm;
struct NodeSource {
  // Borrowed for the source realm's lifetime, never a JS-wrapper owner.
  // Keys are local to this owner and change at its Rekeyed boundary.
  Realm* owner = nullptr;
  NodeKey key;
  Realm* factory = nullptr;
};
}
#endif
