#include "domain/styling/box_layout_internal.h"

namespace ruyijs::domain::styling {
bool BoxFlow::Place(size_t index, const BoxRect& containing, bool definite_height,
                    double x, double y, size_t positioned, unsigned depth) {
  if (depth > 256 || index >= nodes.size()) return false;
  ++placements;
  const auto& node = nodes[index]; auto& box = boxes[index];
  const auto& style = node.style;
  if (style.Get("display") == "none") return true;
  if (node.text) {
    box.present = node.text_width > 0;
    box.border = box.content = {x, y, node.text_width, node.text_height};
    box.line = node.line_height >= 0 ? node.line_height : node.text_height;
    return true;
  }
  box.present = true;
  BoxRect cb = containing;
  const bool absolute = BoxAbsolute(node), fixed = style.Get("position") == "fixed";
  if (absolute) {
    if (positioned == kNoBox || fixed) {
      cb = {fixed ? viewport.scroll_x : 0, fixed ? viewport.scroll_y : 0, viewport.width, viewport.height};
      definite_height = true;
    } else {
      const auto& ancestor = boxes[positioned];
      cb = {ancestor.border[0] + ancestor.edge[3], ancestor.border[1] + ancestor.edge[0],
            ancestor.border[2] - ancestor.edge[1] - ancestor.edge[3],
            ancestor.border[3] - ancestor.edge[0] - ancestor.edge[2]};
      definite_height = true;
    }
  }
  BoxEdges(node, cb[2], &box);
  const double extra_w = box.padding[1]+box.padding[3]+box.edge[1]+box.edge[3];
  const double extra_h = box.padding[0]+box.padding[2]+box.edge[0]+box.edge[2];
  const double percent_h = definite_height ? cb[3] : BoxAuto();
  double width = BoxLength(style, "width", cb[2], BoxAuto());
  double height = BoxLength(style, "height", percent_h, BoxAuto());
  BoxViewportLength(style.Get("width"), viewport.width, viewport.height, &width);
  BoxViewportLength(style.Get("height"), viewport.width, viewport.height, &height);
  // nscoord_MAX is also reflow's NS_UNCONSTRAINEDSIZE block-size sentinel.
  if (height >= kBoxCoordMax / 60) height = BoxAuto();
  // width="100%" / height="100%" attributes map to the dimension properties
  // below the author's own width/height (HTML presentational hints).
  if (!std::isfinite(width) && node.attribute_width_percent >= 0)
    width = BoxRound(node.attribute_width_percent * cb[2] / 100);
  if (!std::isfinite(height) && node.attribute_height_percent >= 0 && std::isfinite(percent_h))
    height = BoxRound(node.attribute_height_percent * percent_h / 100);
  const bool border_box = style.Get("box-sizing") == "border-box";
  if (border_box) { width -= extra_w; height -= extra_h; }
  const double left = BoxLength(style, "left", cb[2], BoxAuto());
  const double right = BoxLength(style, "right", cb[2], BoxAuto());
  const double top = BoxLength(style, "top", cb[3], BoxAuto());
  const double bottom = BoxLength(style, "bottom", cb[3], BoxAuto());
  // A flex item is blockified; its container resolved its size (box_layout_flex.cpp).
  const bool flex_item = index < forced_width.size() && std::isfinite(forced_width[index]);
  const bool inline_box = style.Get("display") == "inline" && !BoxReplaced(node) && !absolute && !flex_item;
  if (inline_box) width = height = BoxAuto();
  if (BoxReplaced(node)) {
    if (!std::isfinite(width)) width = node.intrinsic_width >= 0 ? node.intrinsic_width : 300;
    if (!std::isfinite(height)) height = node.intrinsic_height >= 0 ? node.intrinsic_height : 150;
  }
  if (absolute && !std::isfinite(width) && std::isfinite(left) && std::isfinite(right))
    width = cb[2]-left-right-extra_w-box.margin[1]-box.margin[3];
  if (absolute && !std::isfinite(height) && std::isfinite(top) && std::isfinite(bottom))
    height = cb[3]-top-bottom-extra_h-box.margin[0]-box.margin[2];
  bool auto_width = !std::isfinite(width);
  if (auto_width) width = cb[2]-extra_w-box.margin[1]-box.margin[3];
  // An auto-width inline-flex container is shrink-to-fit around its items.
  if (auto_width && !absolute && style.Get("display") == "inline-flex")
    width = std::min(width, Intrinsic(index, false, depth));
  width = BoxLimit(style, true, cb[2], std::max(0.0, width), extra_w);
  if (flex_item) { width = forced_width[index]; auto_width = false; }
  if (index < forced_height.size() && std::isfinite(forced_height[index])) height = forced_height[index];
  box.definite_height = std::isfinite(height);
  if (box.definite_height && !(index < forced_height.size() && std::isfinite(forced_height[index])))
    height = BoxLimit(style, false, percent_h, std::max(0.0, height), extra_h);
  if (!absolute && !auto_width && !flex_item) {
    const double free = std::max(0.0, cb[2]-width-extra_w-box.margin[1]-box.margin[3]);
    const bool auto_l = style.Get("margin-left") == "auto", auto_r = style.Get("margin-right") == "auto";
    if (auto_l) box.margin[3] = BoxRound(free / (auto_r ? 2 : 1));
    if (auto_r) box.margin[1] = BoxRound(free / (auto_l ? 2 : 1));
  }
  double bx = x + box.margin[3], by = y + box.margin[0];
  if (absolute) {
    if (std::isfinite(left)) bx = cb[0]+left+box.margin[3];
    else if (std::isfinite(right)) bx = cb[0]+cb[2]-right-width-extra_w-box.margin[1];
    if (std::isfinite(top)) by = cb[1]+top+box.margin[0];
    else if (std::isfinite(bottom) && box.definite_height) by = cb[1]+cb[3]-bottom-height-extra_h-box.margin[2];
  } else if (style.Get("position") == "relative") {
    bx += std::isfinite(left) ? left : std::isfinite(right) ? -right : 0;
    by += std::isfinite(top) ? top : std::isfinite(bottom) ? -bottom : 0;
  }
  box.border = {BoxRound(bx), BoxRound(by), BoxRound(width+extra_w), BoxRound((box.definite_height?height:0)+extra_h)};
  box.content = {box.border[0]+box.edge[3]+box.padding[3], box.border[1]+box.edge[0]+box.padding[0],
                 width, box.definite_height ? height : 0};
  const bool establishes = style.Get("position") != "static" || style.Get("transform") != "none";
  const size_t child_positioned = establishes ? index : positioned;
  double children_height = 0, children_width = 0;
  if (!BoxReplaced(node) && !Children(index, child_positioned, depth, &children_height, &children_width)) return false;
  if (inline_box || (auto_width && style.Get("display") == "inline-block")) {
    box.content[2] = children_width; box.border[2] = BoxRound(children_width+extra_w);
  }
  if (absolute && auto_width && !BoxReplaced(node)) {
    // Shrink-to-fit: min(max(min-content, available), max-content). Without
    // a wrap opportunity the min-content width is the unwrapped line itself.
    bool wraps = false;
    for (const auto child : node.children) wraps = wraps || nodes[child].breakable;
    const double minimum = wraps ? 0 : children_width;
    const double fit = BoxLimit(style, true, cb[2],
                                std::min(std::max(minimum, box.content[2]), children_width), extra_w);
    box.content[2] = fit; box.border[2] = BoxRound(fit+extra_w);
  }
  if (inline_box) {
    // A non-replaced inline box is as tall as its font's content area; its
    // line-height contribution is what its contents add to the line box.
    box.line = children_height;
    double area = 0;
    for (const auto child : node.children) {
      const auto& item = nodes[child];
      if (item.text) area = std::max(area, item.text_height);
      else if (!BoxAbsolute(item) && item.style.Get("display") == "inline" && !BoxReplaced(item))
        area = std::max(area, boxes[child].content[3]);
    }
    if (area > 0) children_height = area;
  }
  if (!box.definite_height) {
    box.content[3] = BoxLimit(style, false, percent_h, children_height, extra_h);
    box.border[3] = BoxRound(box.content[3]+extra_h);
  }
  for (const auto child : node.children)
    if (BoxAbsolute(nodes[child]) && !Place(child, box.content, box.definite_height,
        std::isfinite(static_x[child]) ? static_x[child] : box.content[0],
        std::isfinite(static_y[child]) ? static_y[child] : box.content[1],
        child_positioned, depth+1)) return false;
  return true;
}
bool ComputeBoxLayout(const std::vector<BoxNode>& nodes, const BoxViewport& viewport,
                       std::vector<BoxLayout>* result) {
  if (nodes.empty() || nodes.size() > 16384) return false;
  result->assign(nodes.size(), {});
  BoxFlow flow{nodes, viewport, *result};
  flow.forced_width.assign(nodes.size(), BoxAuto());
  flow.forced_height.assign(nodes.size(), BoxAuto());
  flow.max_content.assign(nodes.size(), BoxAuto());
  flow.min_content.assign(nodes.size(), BoxAuto());
  flow.static_x.assign(nodes.size(), BoxAuto());
  flow.static_y.assign(nodes.size(), BoxAuto());
  return flow.Place(0, {0, 0, viewport.width, viewport.height}, true, 0, 0, kNoBox, 0);
}
}
