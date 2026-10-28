#ifndef RUYIJS_BROWSER_ENVIRONMENT_VISUAL_VIEWPORT_H
#define RUYIJS_BROWSER_ENVIRONMENT_VISUAL_VIEWPORT_H
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::browser_environment {
struct State;
// The VisualViewport interface: one realm-local EventTarget-derived instance
// behind window.visualViewport ([SameObject, Replaceable]). Width and height
// read the same screen configuration window.innerWidth/innerHeight do, the page
// offsets read this Context's simulated scroll state, and there is no pinch
// zoom, so offsetLeft/offsetTop are 0 and scale is 1. Exactly one production
// installer for this interface (docs/owners/BROWSER_ENVIRONMENT.md, D47).
bool InstallVisualViewport(State& state, interaction::Realm& events);
}  // namespace ruyijs::browser_environment
#endif
