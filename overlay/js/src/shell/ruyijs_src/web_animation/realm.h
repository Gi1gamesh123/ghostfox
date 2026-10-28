#ifndef RUYIJS_WEB_ANIMATION_REALM_H
#define RUYIJS_WEB_ANIMATION_REALM_H
#include <memory>
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::web_animation {
struct State;
// Web Animations for one Window realm (docs/owners/WEB_ANIMATION.md):
// AnimationTimeline, DocumentTimeline, AnimationEffect, KeyframeEffect,
// Animation, CSSAnimation, CSSTransition, Document.timeline/getAnimations and
// Element.animate/getAnimations. Effects do not apply styles (recorded deviation).
class Realm final {
 public:
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, interaction::Realm& events);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  // Document Model ports; bound before Install, cleared before teardown.
  struct DocumentPorts {
    // 1 Element, 2 Document, 0 anything else (wrappers unwrapped).
    int (*kind)(void*, JS::HandleValue) = nullptr;
    // The node's document wrapper (the document itself for a Document).
    bool (*document)(void*, JS::HandleObject node, JS::MutableHandleObject document) = nullptr;
    // Whether `node` is in `root`'s scope: a Document root holds its connected
    // nodes; an Element root holds itself, and its descendants when `subtree`.
    bool (*inScope)(void*, JS::HandleObject root, JS::HandleObject node, bool subtree, bool* result) = nullptr;
    JSObject* (*mainDocument)(void*) = nullptr;
    void* context = nullptr;
  };
  void SetDocumentPorts(const DocumentPorts& ports);
  // Scheduling ports; bound by the shared window coordinator, cleared before teardown.
  struct SchedulingPorts {
    double (*now)(void*, JS::HandleObject page) = nullptr;  // ms since this page's time origin
    bool (*post)(void*, JS::HandleObject task) = nullptr;
    bool (*postDelayed)(void*, JS::HandleObject task, double ms) = nullptr;
    void* context = nullptr;
  };
  void SetSchedulingPorts(const SchedulingPorts& ports);
  // Installs the interfaces and the four members on the Document Model's
  // Document and Element prototypes.
  bool Install(JS::HandleObject documentPrototype, JS::HandleObject elementPrototype);
 private:
  std::unique_ptr<State> state_;
};
}
#endif
