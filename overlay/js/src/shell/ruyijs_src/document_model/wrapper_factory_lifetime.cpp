// Original factory links share the one native Wrapper and never own JS nodes.
#include "document_model/state.h"
#include "document_model/svg_view_cache.h"
#include "js/Object.h"

namespace ruyijs::document_model {
Wrapper::Wrapper(JSContext* cx, NodeKey id, JSObject* value)
    : key(id), object(value), root(cx), mediaKeys(cx) { mediaKeys.setNull(); }
void Wrapper::LinkFactory(State& source) {
  factory = &source;
  factoryNext = source.factoryWrappers;
  if (factoryNext) factoryNext->factoryPrevious = this;
  source.factoryWrappers = this;
}

Wrapper::~Wrapper() {
  if (!factory) return;
  if (factoryPrevious) factoryPrevious->factoryNext = factoryNext;
  else factory->factoryWrappers = factoryNext;
  if (factoryNext) factoryNext->factoryPrevious = factoryPrevious;
}

void State::RetireFactoryWrappers() {
  while (factoryWrappers) {
    Wrapper* entry = factoryWrappers;
    entry->svgViews.reset();
    factoryWrappers = entry->factoryNext;
    // Interaction belongs to the original factory even after the native node
    // moves. Clear its borrowed slot before that realm's registry is destroyed.
    if (!entry->dead && entry->object)
      JS::SetReservedSlot(entry->object, 1, JS::PrivateValue(nullptr));
    entry->factory = nullptr;
    entry->factoryPrevious = entry->factoryNext = nullptr;
  }
}
}  // namespace ruyijs::document_model
