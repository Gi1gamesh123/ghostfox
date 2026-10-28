#pragma once
#include <array>
#include <vector>
#include "web_geometry/internal.h"
#include "js/Proxy.h"
namespace ruyijs::web_geometry {
extern const JSClass kRectListClass;
const js::BaseProxyHandler* RectListHandler();
bool NewRectList(State&, JS::MutableHandleValue);
bool NewRectList(State&, const double*, bool present, JS::MutableHandleValue);
// A DOMRectList of several rectangles, in order.
bool NewRectList(State&, const std::vector<std::array<double, 4>>&, JS::MutableHandleValue);
uint32_t RectListLength(JSContext*, JS::HandleObject);
bool RectListItem(JSContext*, JS::HandleObject, uint32_t, JS::MutableHandleValue);
bool InstallRectList(State&);
}
