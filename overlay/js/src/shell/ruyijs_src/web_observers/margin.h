#ifndef RUYIJS_WEB_OBSERVERS_MARGIN_H
#define RUYIJS_WEB_OBSERVERS_MARGIN_H
#include "document_model/model.h"
namespace ruyijs::web_observers {
using document_model::DomString;

// rootMargin / scrollMargin of an IntersectionObserver. The donor parses the
// value with its CSS parser restricted to bare px or percent lengths
// (servo/components/style/values/specified/intersection_observer.rs): one to
// four components, an empty input means 0px, and serialization always writes
// all four sides. This owner parses and serializes that one grammar; it never
// keeps a CSS value object of another domain.
struct Margin {
  double top = 0, right = 0, bottom = 0, left = 0;
  bool percent_top = false, percent_right = false, percent_bottom = false,
       percent_left = false;
};

// False on any input the donor's parser rejects (bad unit, a bare number, more
// than four components). The caller raises the SyntaxError, because the error
// message names the caller's own attribute.
bool ParseMargin(const DomString& text, Margin* margin);
DomString SerializeMargin(const Margin& margin);
}  // namespace ruyijs::web_observers
#endif
