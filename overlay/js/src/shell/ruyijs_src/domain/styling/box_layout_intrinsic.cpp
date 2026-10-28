// Intrinsic (min-content / max-content) inline sizes of a box subtree, for flex
// base sizes and automatic minimums (CSS Sizing 3 §5, CSS Flexbox 1 §9.2, §4.5).
// Percentages resolve to zero, so a node's value is independent of its
// containing block and is memoized for the call.
#include <cmath>
#include "domain/styling/box_layout_internal.h"

namespace ruyijs::domain::styling {
namespace {
double Horizontal(const BoxLayout& edges, bool margins) {
  return edges.padding[1] + edges.padding[3] + edges.edge[1] + edges.edge[3] +
         (margins ? edges.margin[1] + edges.margin[3] : 0);
}
bool InlineLevel(const BoxNode& node) {
  if (node.text) return true;
  const auto display = node.style.Get("display");
  return display == "inline" || display == "inline-block" || display == "inline-flex";
}
}  // namespace

double BoxFlow::Intrinsic(size_t index, bool minimum, unsigned depth) {
  if (index >= nodes.size() || depth > 256) return 0;
  auto& memo = minimum ? min_content : max_content;
  if (index < memo.size() && std::isfinite(memo[index])) return memo[index];
  const auto& node = nodes[index];
  double result = 0;
  if (node.text) {
    // The longest word is not measured; breakable text has no min-content floor.
    result = minimum && node.breakable ? 0 : node.text_width;
  } else {
    const auto& style = node.style;
    BoxLayout edges;
    BoxEdges(node, 0, &edges);
    const double extra = Horizontal(edges, false);
    double width = BoxLength(style, "width", BoxAuto(), BoxAuto());
    if (std::isfinite(width) && style.Get("box-sizing") == "border-box") width = std::max(0.0, width - extra);
    result = std::isfinite(width) ? width : IntrinsicContent(index, minimum, depth);
    result = BoxLimit(style, true, BoxAuto(), result, extra);
  }
  if (index < memo.size()) memo[index] = result;
  return result;
}

double BoxFlow::IntrinsicContent(size_t index, bool minimum, unsigned depth) {
  if (index >= nodes.size() || depth > 256) return 0;
  const auto& node = nodes[index];
  if (node.text) return Intrinsic(index, minimum, depth);
  if (BoxReplaced(node)) return node.intrinsic_width >= 0 ? node.intrinsic_width : 300;
  const auto& style = node.style;
  double result = 0;
  const auto display = style.Get("display");
  const bool flex = display == "flex" || display == "inline-flex";
  const auto direction = style.Get("flex-direction");
  const bool row = flex && direction.rfind("column", 0) != 0;
  const auto wrap = style.Get("flex-wrap");
  const bool wraps = wrap == "wrap" || wrap == "wrap-reverse";
  const auto gap_text = style.Get("column-gap");
  const double gap = gap_text.empty() || gap_text == "normal" ? 0 : BoxLength(style, "column-gap", BoxAuto(), 0);
  double run = 0;
  size_t count = 0;
  for (const auto child : node.children) {
    const auto& item = nodes[child];
    if (BoxAbsolute(item) || item.style.Get("display") == "none") continue;
    if (item.text && item.text_width <= 0) continue;
    const double outer = IntrinsicOuter(child, minimum, depth + 1);
    if (row) {
      if (minimum && wraps) result = std::max(result, outer);
      else result += outer + (count ? gap : 0);
    } else if (flex || !InlineLevel(item) || minimum) {
      run = 0;
      result = std::max(result, outer);
    } else {
      run += outer;
      result = std::max(result, run);
    }
    ++count;
  }
  return result;
}

double BoxFlow::IntrinsicOuter(size_t index, bool minimum, unsigned depth) {
  const auto& node = nodes[index];
  if (node.text) return Intrinsic(index, minimum, depth);
  BoxLayout edges;
  BoxEdges(node, 0, &edges);
  return Intrinsic(index, minimum, depth) + Horizontal(edges, true);
}
}  // namespace ruyijs::domain::styling
