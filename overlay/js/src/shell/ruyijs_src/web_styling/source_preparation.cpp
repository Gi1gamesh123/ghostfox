// Source membership, external load discovery and ready filtering over one cache.
#include "web_styling/cssom.h"
#include "domain/styling/external_style_value.h"
#include <algorithm>
namespace ruyijs::web_styling {
bool PrepareSheetSources(State& state, std::vector<document_model::StyleSheetSource>* sources,
                          const document_model::StyleSheetScope* scope, bool readyOnly,
                          bool retireMembership) {
  using namespace ruyijs::domain::styling;
  sources->erase(std::remove_if(sources->begin(),sources->end(),[](const auto& source) {
    return source.external && (!source.presented || source.disabled || source.href.empty() ||
        !StyleRelation(source.rel,"stylesheet") || !StyleType(source.typeAttribute) ||
        (StyleRelation(source.rel,"alternate") && source.titleAttribute.empty()));
  }),sources->end());
  for (const auto& source : *sources)
    if (!source.external || !StyleRelation(source.rel,"alternate"))
      state.sheetSets[{source.scopeDocument,source.scopeNode}].observe(source.title);
  // Pending sources participate in membership; reading a list must not cancel them.
  if (retireMembership && !state.RetireSheets(*sources,scope)) return false;
  for (const auto& source : *sources) {
    if (!source.external) continue;
    auto cache = state.Materialize(source);
    if (!cache || !EnsureExternalSheet(state,cache)) return false;
  }
  if (readyOnly) sources->erase(std::remove_if(sources->begin(),sources->end(),[&](const auto& source) {
    if (!source.external) return false;
    auto cache = state.Materialize(source);
    return !cache || !cache->ready;
  }),sources->end());
  return true;
}
}
