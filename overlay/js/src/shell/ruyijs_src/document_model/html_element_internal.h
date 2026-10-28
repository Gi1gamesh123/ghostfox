// Native entry points shared by html_element.cpp (reflection + installer) and
// html_element_actions.cpp (style/dataset facades and click). Owner:
// document_model (T4).
#ifndef RUYIJS_DOCUMENT_MODEL_HTML_ELEMENT_INTERNAL_H
#define RUYIJS_DOCUMENT_MODEL_HTML_ELEMENT_INTERNAL_H
#include "js/Value.h"
#include "js/RootingAPI.h"

namespace ruyijs::document_model {
struct State;
struct Wrapper;
Wrapper* HTMLReceiver(JSContext*, JS::HandleValue, State& function, const JS::CallArgs* args = nullptr);
bool StyleElementGetter(JSContext*, unsigned, JS::Value*);
bool StyleElementSetter(JSContext*, unsigned, JS::Value*);
bool DatasetElementGetter(JSContext*, unsigned, JS::Value*);
bool ClickElement(JSContext*, unsigned, JS::Value*);
bool InertElement(JSContext*, unsigned, JS::Value*);
// HTMLElement.prototype.focus/blur over the focus owner (focus.cpp).
bool FocusMethod(JSContext*, unsigned, JS::Value*);
bool BlurMethod(JSContext*, unsigned, JS::Value*);
}  // namespace ruyijs::document_model
#endif
