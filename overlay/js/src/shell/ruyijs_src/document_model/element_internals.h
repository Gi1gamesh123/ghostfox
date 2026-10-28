#ifndef RUYIJS_DOCUMENT_ELEMENT_INTERNALS_H
#define RUYIJS_DOCUMENT_ELEMENT_INTERNALS_H
// Shared by element_internals.cpp (attachInternals, setValidity) and
// element_internals_members.cpp (the ElementInternals members).
#include <string>
#include "document_model/state.h"

namespace ruyijs::document_model {
enum InternalsSlot { kElement, kFlags, kMessage, kValue, kFormState, kStates, kValidity, kLabels, kAria,
                     kInternalsSlots };
extern const JSClass kInternalsClass;
JSObject* InternalsSelf(JS::HandleValue value);
bool InternalsIsElement(State& s, JS::HandleValue value);
CustomElementDefinition* InternalsDefinition(State& s, JSObject* internals);
bool InternalsFail(State& s, const std::string& message);  // NotSupportedError
bool AttachInternals(JSContext* cx, unsigned argc, JS::Value* vp);
bool InternalsSetValidity(State& s, JS::HandleObject self, const JS::CallArgs& args);
}  // namespace ruyijs::document_model
#endif
