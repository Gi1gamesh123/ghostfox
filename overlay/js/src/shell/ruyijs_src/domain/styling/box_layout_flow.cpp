#include "domain/styling/box_layout_internal.h"

namespace ruyijs::domain::styling {
void BoxFlow::ShiftBoxes(size_t index, double dx, double dy) {
  if (dx == 0 && dy == 0) return;
  auto& box = boxes[index];
  box.border[0] += dx; box.border[1] += dy;
  box.content[0] += dx; box.content[1] += dy;
  for (const auto child : nodes[index].children) ShiftBoxes(child, dx, dy);
}
bool BoxFlow::Children(size_t index, size_t positioned, unsigned depth, double* height, double* width) {
  const auto display = nodes[index].style.Get("display");
  if (display == "flex" || display == "inline-flex")
    return FlexChildren(index, positioned, depth, height, width);
  if (display == "grid" || display == "inline-grid")
    return GridChildren(index, positioned, depth, height, width);
  const auto& parent = boxes[index];
  double cursor_y = 0, line_x = 0, line_height = 0, extent_x = 0, previous_margin = 0;
  for (const auto child : nodes[index].children) {
    const auto& node = nodes[child];
    const auto display = node.style.Get("display");
    if (display == "none") continue;
    if (BoxAbsolute(node)) {
      // CSS static position: the position this child would have occupied in
      // normal flow. Do not consume space; resolve it after the parent size.
      const bool inline_position = display == "inline" || display == "inline-block";
      static_x[child] = parent.content[0] + (inline_position ? line_x : 0);
      static_y[child] = parent.content[1] + cursor_y + (inline_position ? 0 : line_height);
      continue;
    }
    const bool in_line = node.text || display == "inline" || display == "inline-block";
    if (!in_line) { cursor_y += line_height; line_x = line_height = 0; }
    const double x = parent.content[0]+(in_line ? line_x : 0);
    // Anonymous text boxes inherit font properties, but element margins,
    // padding, and borders do not apply to their inline content.
    if (node.text) {
      boxes[child].margin = {};
      boxes[child].padding = {};
      boxes[child].edge = {};
    } else {
      BoxEdges(node, parent.content[2], &boxes[child]);
    }
    const double margin = boxes[child].margin[0];
    // Adjacent block margins collapse; negative margins participate by minimum.
    const double collapse = in_line ? 0 : previous_margin+margin-
        (std::max(0.0,std::max(previous_margin,margin))+std::min(0.0,std::min(previous_margin,margin)));
    const double y = parent.content[1]+cursor_y-collapse;
    if (!Place(child, parent.content, parent.definite_height, x, y, positioned, depth+1)) return false;
    const auto& box = boxes[child];
    if (!box.present) continue;
    const double outer_width = box.border[2]+box.margin[1]+box.margin[3];
    const double outer_height = box.border[3]+box.margin[0]+box.margin[2];
    if (in_line) {
      line_x += outer_width;
      // An inline replaced element is baseline-aligned. Firefox's line box
      // keeps the normal-font descent below an iframe/image baseline; without
      // it the containing block is 5.5px too short for the Turnstile iframe.
      const double baseline_descent = BoxReplaced(node) ? 5.5 : 0.0;
      const bool inline_flow = node.text || (display == "inline" && !BoxReplaced(node));
      line_height = std::max(line_height, inline_flow ? box.line : outer_height + baseline_descent);
      extent_x = std::max(extent_x,line_x); previous_margin = 0;
    } else {
      cursor_y += outer_height-collapse;
      extent_x = std::max(extent_x,outer_width); previous_margin = box.margin[2];
    }
  }
  *height = BoxRound(cursor_y+line_height); *width = BoxRound(extent_x);
  return true;
}

}
