#include "document_model/state.h"
#include "document_model/exception.h"
#include <new>

namespace ruyijs::document_model {
bool InstallException(State& state) {
  std::unique_ptr<Interface> entry(new (std::nothrow) Interface(state.cx, "DOMException"));
  if (!entry) { JS_ReportOutOfMemory(state.cx);return false; }
  JS::RootedObject prototype(state.cx), constructor(state.cx);
  if (!InstallExceptionSurface(state.cx, state.page, &prototype, &constructor)) return false;
  entry->prototype = prototype;entry->constructor = constructor;
  entry->next = std::move(state.interfaces);state.interfaces = std::move(entry);
  return true;
}
bool ThrowDOM(State& state, const char* name, const char* message) {
  JS::RootedObject prototype(state.cx, state.Find("DOMException")->prototype);
  return ThrowException(state.cx, prototype, name, message);
}
}
