#include "document_model/state.h"
#include "domain/document_model/foreign_element_interfaces.h"

namespace ruyijs::document_model {
bool InstallForeignElementInterfaces(State& state) {
  JS::RootedObject parent(state.cx);
  for (const auto& row : ruyijs::domain::document_model::kSvgElementInterfaces) {
    parent = state.Find(row.parent)->prototype;
    if (!state.DefineInterface(row.name, parent, Illegal)) return false;
  }
  parent = state.Find("Element")->prototype;
  return state.DefineInterface("MathMLElement", parent, Illegal);
}
}
