#pragma once
namespace ruyijs {
struct BrowsingContext;
// Element scrolling ports (element_scroll_ports.cpp), bound with the geometry ports.
void BindElementScroll(BrowsingContext&);
void UnbindElementScroll(BrowsingContext&);
}  // namespace ruyijs
