// DOMParser / XMLSerializer: platform services built on the existing HTML
// tokenizer/tree-builder and the existing DomTreeService serializer. No new
// parser, no second tree owner. Owner: the document_model bridge.
#ifndef RUYIJS_DOCUMENT_PARSING_H
#define RUYIJS_DOCUMENT_PARSING_H

#include "js/CallArgs.h"
#include "js/Class.h"
#include "js/RootingAPI.h"

namespace ruyijs::document_model {
struct State;

// One native class behind both services; reserved slot 0 holds the owning
// State (realm brand), slot 1 the service id (0 parser, 1 serializer, 2
// implementation), slot 2 the original source Document for implementation.
extern const JSClass kServiceClass;

// Creates a service object of this realm; lets `new DOMParser()` succeed while
// the receiver brand keeps a foreign prototype out.
bool NewService(State&, int service, JS::MutableHandleValue result, const JS::CallArgs* args = nullptr);
// document.implementation: one cached service per original Document wrapper.
bool InstallImplementationObject(State&);
bool InstallParsing(State&);
}  // namespace ruyijs::document_model
#endif
