#ifndef RUYIJS_DOCUMENT_ATTR_OBJECT_H
#define RUYIJS_DOCUMENT_ATTR_OBJECT_H
#include <optional>
#include "js/Class.h"
#include "js/RootingAPI.h"
#include "document_model/model.h"

namespace ruyijs::document_model {
struct State;

// One attribute object is a live view of one attribute of one element, keyed by
// namespace and local name, so two attributes that share a qualified name in
// different namespaces stay distinct. Every field is read from the owner on
// access; the slot keeps only the identity and the last known spelling.
struct AttrSlot {
  NodeKey element;  // Refreshed from the source Node slot; null while detached.
  DomString namespace_uri, local_name, name, prefix;
  std::optional<DomString> detached_value;  // Only set while element is null.
};

extern const JSClass kAttrClass;
// Original native object/slot after checked cross-realm unwrapping.
JSObject* AttrObject(JS::HandleValue);
AttrSlot* AttrSlotOf(JS::HandleValue);
// Source owner for a validated Attr; detached values need no document state.
State& AttrOwner(State& fallback, JS::HandleValue);
// Installs the Attr prototype accessors; attr_object.cpp is their only owner.
bool InstallAttr(State&);
}  // namespace ruyijs::document_model
#endif
