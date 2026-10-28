#ifndef RUYIJS_DOCUMENT_MODEL_IFRAME_ATTRIBUTES_H
#define RUYIJS_DOCUMENT_MODEL_IFRAME_ATTRIBUTES_H

namespace ruyijs::document_model {
class State;
// Installs HTMLIFrameElement's two reflected content attributes (`src` and
// `srcdoc`). The frame-lifecycle installer owns the element-to-context links;
// this TU owns only the attribute reflectors, so each descriptor still has
// exactly one production installer (see docs/owners/IFRAME.md).
bool InstallIframeAttributes(State& state);
bool InstallIframeMembers(State& state);
}  // namespace ruyijs::document_model

#endif
