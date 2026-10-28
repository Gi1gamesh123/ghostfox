// Per-document CSS list sources from the existing DOM port and original cache.
#include "web_styling/cssom.h"

namespace ruyijs::web_styling {
bool ReadDocumentSheetSources(State& state, JS::HandleObject document,
                               std::vector<document_model::StyleSheetSource>* out) {
  if (!state.documentSources) return WebKit::Fail(state.cx, "Document CSS source is not installed");
  document_model::StyleSheetScope scope;
  if (!state.documentSources(state.documentContext, document, &scope, out)) return false;
  return PrepareSheetSources(state, out, &scope);
}
}
