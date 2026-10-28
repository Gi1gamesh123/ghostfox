// CSS Custom Highlight API (Firefox 158 Highlight.webidl, HighlightRegistry.webidl):
// Highlight is a setlike<AbstractRange> with priority/type; HighlightRegistry is a
// maplike<DOMString, Highlight> behind CSS.highlights (one per realm). Each object's
// one state is its backing engine Set/Map, so iterators are the engine's, as in
// Firefox. Ranges are recognised through the Document Model's range brand port;
// nothing is painted (D148).
#ifndef RUYIJS_WEB_STYLING_HIGHLIGHT_H
#define RUYIJS_WEB_STYLING_HIGHLIGHT_H
#include "js/RootingAPI.h"
struct JSContext;
namespace ruyijs::web_styling {
struct State;
// The shared member body of both interfaces: brand, arguments, then the engine
// collection operation. `iface` names the interface in Firefox's error texts and
// `name` the operation ("size" for the size getter).
bool CollectionMember(JSContext* cx, unsigned argc, JS::Value* vp, const JSClass* clasp, const char* iface,
                      const char* name);
bool IsHighlight(JS::HandleValue value);
bool InstallHighlight(State& state);
bool InstallHighlightRegistry(State& state, JS::HandleObject css);
}  // namespace ruyijs::web_styling
#endif
