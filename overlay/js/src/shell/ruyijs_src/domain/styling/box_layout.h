#pragma once
#include <array>
#include <string>
#include <utility>
#include <vector>

namespace ruyijs::domain::styling {
using BoxRect = std::array<double, 4>;
struct BoxStyle {
  std::vector<std::pair<std::string, std::string>> values;
  double font_size = 16, root_font_size = 16;
  unsigned specified_margin_mask = 0, specified_border_mask = 0, specified_padding_mask = 0;
  std::string Get(const char* name) const;
};
struct BoxNode {
  BoxStyle style;
  std::string tag, input_type;
  std::vector<size_t> children;
  size_t parent = size_t(-1);
  bool text = false;
  bool breakable = false;  // Text with a soft wrap opportunity (min-content < max-content).
  // text_height is the primary font content area (inline box height);
  // line_height is the text's contribution to its line box.
  double text_width = 0, text_height = 0, line_height = -1, intrinsic_width = -1, intrinsic_height = -1;
  // HTML width/height attribute percentages (presentational hints that apply
  // when the author's width/height is auto), and an iframe frameborder="0"/"no".
  double attribute_width_percent = -1, attribute_height_percent = -1;
  bool no_frame_border = false;
};
struct BoxViewport { double width = 0, height = 0, scroll_x = 0, scroll_y = 0; };
struct BoxLayout {
  BoxRect border{}, content{};
  std::array<double, 4> margin{}, padding{}, edge{};
  double line = 0;  // Line box contribution of text and non-replaced inline boxes.
  bool present = false, definite_height = false;
};
// Call-scoped projection: no node identity, retained tree, renderer or engine dependency.
bool ComputeBoxLayout(const std::vector<BoxNode>&, const BoxViewport&, std::vector<BoxLayout>*);
BoxRect ProjectBoxRect(const std::vector<BoxNode>&, const std::vector<BoxLayout>&,
                       const BoxViewport&, size_t target);
}
