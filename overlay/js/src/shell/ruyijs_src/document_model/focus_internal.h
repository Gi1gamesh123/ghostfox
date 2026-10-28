#pragma once
// Shared by focus.cpp and user_actions.cpp (ARCHITECTURE.md, host user input).
#include "document_model/state.h"
namespace ruyijs::document_model::focus_detail {
bool Has(State&, NodeKey, const char16_t* name, DomString* value = nullptr);
DomString Lower(DomString);
DomString Tag(const NodeSnapshot&);
bool Fire(State&, NodeKey, const char* type, bool bubbles, JS::HandleValue related);
// Firefox's focusable set for script and pointer focus, without the rendering test.
bool Focusable(State&, NodeKey);
// Moves `document`'s focus to `next`; a null `next` clears it.
bool MoveFocus(State&, NodeKey document, NodeKey next);
NodeKey OwnerDocument(State&, NodeKey);
NodeKey PointerFocusTarget(State&, NodeKey);
Wrapper* Element(JSContext*, JS::HandleObject);
}
