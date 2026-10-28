#ifndef RUYIJS_DOCUMENT_CUSTOM_ELEMENT_REACTIONS_H
#define RUYIJS_DOCUMENT_CUSTOM_ELEMENT_REACTIONS_H
// Helpers shared by custom_element_reactions.cpp and custom_element_attributes.cpp.
#include "document_model/state.h"

namespace ruyijs::document_model::reactions {
void Settle(State& s);  // reports a pending exception (sinks have no caller to throw to)
bool Usable(State& s);  // a registry exists, no pending exception, nesting below the limit
void Invoke(State& s, NodeKey key, JS::HandleValue callback, JS::HandleValueArray args);
CustomElementState* StateOf(State& s, NodeKey key);
}  // namespace ruyijs::document_model::reactions
#endif
