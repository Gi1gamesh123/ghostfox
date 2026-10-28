// Grid layout (CSS Grid 1): explicit tracks from grid-template-columns/rows
// (lengths, percentages, fr, auto, min-/max-content, minmax(), repeat() with a
// count, auto-fill or auto-fit), implicit rows from grid-auto-rows, row-major
// auto-placement with line numbers and spans, the track sizing algorithm
// (§12: base sizes, growth limits, maximize, flexible tracks, stretch auto),
// gaps, and align-self/align-items in the row. Implicit columns and named
// lines/areas are not resolved: such an item takes the next auto position.
#include "domain/styling/box_layout_internal.h"
#include "domain/styling/css_numeric_value.h"
#include "domain/styling/css_components.h"
#include <cstdlib>

namespace ruyijs::domain::styling {
namespace {
constexpr size_t kMaxTracks = 1000;
struct Track { double min = 0, max = 0, fr = 0; bool min_auto = false, max_auto = false; double base = 0, limit = 0; };
std::vector<std::string> Words(const std::string& text) {
  std::vector<std::string> out;
  int depth = 0;
  std::string word;
  for (char c : text) {
    if (c == '(') ++depth;
    if (c == ')') --depth;
    if ((c == ' ' || c == '\t' || c == '\n') && depth == 0) { if (!word.empty()) out.push_back(word); word.clear(); }
    else word.push_back(c);
  }
  if (!word.empty()) out.push_back(word);
  return out;
}
bool Length(const BoxStyle& style, const std::string& text, double percent, double* out) {
  if (text.find('%') != text.npos && !std::isfinite(percent)) return false;
  const auto value = CssNumericResolve(text, style.font_size, style.root_font_size, std::isfinite(percent) ? percent : 0);
  if (!value.kind || (value.kind != CssLength && value.number != 0)) return false;
  *out = std::max(0.0, value.number);
  return true;
}
// One <track-size>: a breadth or minmax(min, max).
Track Size(const BoxStyle& style, const std::string& text, double percent) {
  Track track;
  auto side = [&](const std::string& part, bool maximum) {
    double value = 0;
    if (part.size() > 2 && part.compare(part.size() - 2, 2, "fr") == 0) {
      if (maximum) track.fr = std::max(0.0, std::strtod(part.c_str(), nullptr));
      else track.min_auto = true;  // An fr minimum behaves as auto.
      return;
    }
    if (Length(style, part, percent, &value)) { (maximum ? track.max : track.min) = value; return; }
    (maximum ? track.max_auto : track.min_auto) = true;  // auto, min-content, max-content, fit-content
  };
  if (text.rfind("minmax(", 0) == 0 && text.back() == ')') {
    const auto inner = text.substr(7, text.size() - 8);
    const auto comma = inner.find(',');
    side(CssTrim(inner.substr(0, comma)), false);
    side(CssTrim(comma == inner.npos ? std::string() : inner.substr(comma + 1)), true);
  } else {
    side(text, true);
    if (track.fr > 0) track.min_auto = true;
    else { track.min = track.max; track.min_auto = track.max_auto; }
  }
  return track;
}
std::vector<Track> Tracks(const BoxStyle& style, const std::string& text, double room, double gap) {
  std::vector<Track> out;
  if (text.empty() || text == "none") return out;
  for (const auto& word : Words(text)) {
    if (word.front() == '[') continue;  // line names
    if (word.rfind("repeat(", 0) != 0 || word.back() != ')') { out.push_back(Size(style, word, room)); continue; }
    const auto inner = word.substr(7, word.size() - 8);
    const auto comma = inner.find(',');
    if (comma == inner.npos) continue;
    const auto how = CssTrim(inner.substr(0, comma));
    std::vector<Track> list;
    for (const auto& part : Words(CssTrim(inner.substr(comma + 1))))
      if (part.front() != '[') list.push_back(Size(style, part, room));
    if (list.empty()) continue;
    size_t count = std::strtoul(how.c_str(), nullptr, 10);
    if (how == "auto-fill" || how == "auto-fit") {
      double each = 0;
      for (const auto& t : list) each += (t.max_auto || t.fr > 0 ? t.min : std::max(t.min, t.max)) + gap;
      count = each > 0 && std::isfinite(room) ? std::max<size_t>(1, size_t((room + gap) / each)) : 1;
    }
    for (size_t i = 0; i < count && out.size() < kMaxTracks; ++i) out.insert(out.end(), list.begin(), list.end());
  }
  if (out.size() > kMaxTracks) out.resize(kMaxTracks);
  return out;
}
// A line placement: start line (1-based, 0 = auto) and span.
void Line(const BoxStyle& style, const char* axis, size_t* start, size_t* span) {
  auto start_text = style.Get((std::string("grid-") + axis + "-start").c_str());
  auto end_text = style.Get((std::string("grid-") + axis + "-end").c_str());
  const auto shorthand = style.Get((std::string("grid-") + axis).c_str());
  if (!shorthand.empty() && (start_text.empty() || start_text == "auto")) {
    const auto slash = shorthand.find('/');
    start_text = CssTrim(shorthand.substr(0, slash));
    if (slash != shorthand.npos) end_text = CssTrim(shorthand.substr(slash + 1));
  }
  *start = 0; *span = 1;
  const auto parse_span = [](const std::string& t) { return t.rfind("span", 0) == 0 ? std::max(1L, std::strtol(t.c_str() + 4, nullptr, 10)) : 0L; };
  if (const long s = parse_span(start_text)) *span = size_t(s);
  else if (const long n = std::strtol(start_text.c_str(), nullptr, 10); n > 0) *start = size_t(n);
  if (const long s = parse_span(end_text)) *span = size_t(s);
  else if (const long n = std::strtol(end_text.c_str(), nullptr, 10); n > 0 && *start && size_t(n) > *start) *span = size_t(n) - *start;
}
// §12.4-12.7 for one axis: base sizes from fixed minimums or item minimum
// contributions, growth limits, then free space to non-flexible and fr tracks.
void Size(std::vector<Track>& tracks, double room, double gap, bool stretch) {
  double used = gap * (tracks.empty() ? 0 : tracks.size() - 1), fr = 0;
  for (auto& t : tracks) { t.limit = std::max(t.base, t.limit); used += t.base; fr += t.fr; }
  if (!std::isfinite(room)) {
    for (auto& t : tracks) if (!t.fr) t.base = t.limit;
    return;
  }
  double free = room - used;
  double growable = 0;
  for (auto& t : tracks) if (!t.fr && std::isfinite(t.limit)) growable += t.limit - t.base;
  if (free > 0 && growable > 0) {
    const double share = std::min(1.0, free / growable);
    for (auto& t : tracks) if (!t.fr && std::isfinite(t.limit)) { const double add = (t.limit - t.base) * share; t.base += add; free -= add; }
  }
  if (fr > 0) {
    // §12.7.1: a track whose base exceeds its fr share is inflexible.
    std::vector<bool> flexible(tracks.size());
    for (size_t i = 0; i < tracks.size(); ++i) flexible[i] = tracks[i].fr > 0;
    for (size_t round = 0; round <= tracks.size(); ++round) {
      double leftover = room - gap * (tracks.size() - 1), factors = 0;
      for (size_t i = 0; i < tracks.size(); ++i) if (flexible[i]) factors += tracks[i].fr; else leftover -= tracks[i].base;
      const double unit = std::max(0.0, leftover) / std::max(1.0, factors);
      bool changed = false;
      for (size_t i = 0; i < tracks.size(); ++i)
        if (flexible[i] && tracks[i].base > unit * tracks[i].fr) { flexible[i] = false; changed = true; }
      if (changed) continue;
      for (size_t i = 0; i < tracks.size(); ++i) if (flexible[i]) tracks[i].base = unit * tracks[i].fr;
      break;
    }
  } else if (stretch && free > 0) {
    size_t autos = 0;
    for (const auto& t : tracks) autos += t.max_auto;
    for (auto& t : tracks) if (t.max_auto) t.base += free / autos;
  }
}
double Sum(const std::vector<Track>& t, size_t from, size_t count, double gap) {
  double total = gap * (count ? count - 1 : 0);
  for (size_t i = from; i < from + count && i < t.size(); ++i) total += t[i].base;
  return total;
}
}  // namespace

bool BoxFlow::GridChildren(size_t index, size_t positioned, unsigned depth, double* height, double* width) {
  const auto& style = nodes[index].style;
  const BoxRect content = boxes[index].content;
  const bool definite = boxes[index].definite_height;
  const auto gap = [&](const char* name, double basis) {
    const auto text = style.Get(name);
    double value = 0;
    return text.empty() || text == "normal" || !Length(style, text, basis, &value) ? 0.0 : value;
  };
  const double column_gap = gap("column-gap", content[2]);
  const double row_gap = gap("row-gap", definite ? content[3] : BoxAuto());
  auto columns = Tracks(style, style.Get("grid-template-columns"), content[2], column_gap);
  if (columns.empty()) { Track t; t.min_auto = t.max_auto = true; columns.push_back(t); }
  auto rows = Tracks(style, style.Get("grid-template-rows"), definite ? content[3] : BoxAuto(), row_gap);
  const auto auto_rows = Tracks(style, style.Get("grid-auto-rows").empty() ? "auto" : style.Get("grid-auto-rows"), BoxAuto(), 0);
  struct Item { size_t node, column = 0, row = 0, column_span = 1, row_span = 1; long order = 0; };
  std::vector<Item> items;
  for (const auto child : nodes[index].children) {
    const auto& node = nodes[child];
    if (BoxAbsolute(node) || node.style.Get("display") == "none" || (node.text && node.text_width <= 0)) continue;
    Item item{child};
    if (!node.text) {
      size_t start = 0;
      Line(node.style, "column", &start, &item.column_span);
      item.column = start;
      Line(node.style, "row", &start, &item.row_span);
      item.row = start;
      item.order = std::strtol(node.style.Get("order").c_str(), nullptr, 10);
    }
    item.column_span = std::min(item.column_span, columns.size());
    items.push_back(item);
  }
  std::stable_sort(items.begin(), items.end(), [](const auto& a, const auto& b) { return a.order < b.order; });
  // Row-major auto-placement (§8.5, sparse).
  std::vector<std::vector<bool>> taken;
  const auto free_at = [&](size_t r, size_t c, const Item& item) {
    if (c + item.column_span > columns.size()) return false;
    for (size_t i = r; i < r + item.row_span; ++i)
      for (size_t j = c; j < c + item.column_span; ++j)
        if (i < taken.size() && taken[i][j]) return false;
    return true;
  };
  size_t cursor_row = 0, cursor_column = 0;
  for (auto& item : items) {
    size_t r = item.row ? item.row - 1 : cursor_row, c = item.column ? std::min(item.column - 1, columns.size() - item.column_span) : (item.row ? 0 : cursor_column);
    while (!free_at(r, c, item)) {
      if (item.column) { ++r; continue; }
      if (++c + item.column_span > columns.size()) { c = 0; ++r; }
      if (r > 100000) break;
    }
    item.row = r; item.column = c;
    if (taken.size() < r + item.row_span) taken.resize(r + item.row_span, std::vector<bool>(columns.size()));
    for (size_t i = r; i < r + item.row_span; ++i)
      for (size_t j = c; j < c + item.column_span; ++j) taken[i][j] = true;
    cursor_row = r; cursor_column = c + item.column_span;
  }
  while (rows.size() < taken.size() && rows.size() < 100000) rows.push_back(auto_rows[(rows.size()) % auto_rows.size()]);
  const auto normal = [](const std::string& v) { return v.empty() || v == "normal" || v == "stretch"; };
  // Columns: content contributions of single-span items set auto minimums/maximums.
  for (auto& t : columns) { t.base = t.min_auto ? 0 : t.min; t.limit = t.max_auto ? 0 : (t.fr ? INFINITY : std::max(t.max, t.base)); }
  for (const auto& item : items) {
    if (item.column_span != 1) continue;
    auto& t = columns[item.column];
    if (t.min_auto) t.base = std::max(t.base, IntrinsicOuter(item.node, true, depth + 1));
    if (t.max_auto) t.limit = std::max(t.limit, IntrinsicOuter(item.node, false, depth + 1));
  }
  for (auto& t : columns) if (t.max_auto && !t.fr) t.limit = std::max(t.limit, t.base);
  Size(columns, content[2], column_gap, normal(style.Get("justify-content")));
  // Lay each item out at its column area's width; rows from their heights.
  std::vector<double> outer(items.size());
  for (size_t n = 0; n < items.size(); ++n) {
    const auto& item = items[n];
    auto& box = boxes[item.node];
    const double area = Sum(columns, item.column, item.column_span, column_gap);
    if (!nodes[item.node].text) {
      BoxEdges(nodes[item.node], area, &box);
      const double extra = box.padding[1] + box.padding[3] + box.edge[1] + box.edge[3];
      const auto& cs = nodes[item.node].style;
      double specified = BoxLength(cs, "width", area, BoxAuto());
      if (std::isfinite(specified) && cs.Get("box-sizing") == "border-box") specified -= extra;
      forced_width[item.node] = std::isfinite(specified) ? specified
          : std::max(0.0, area - box.margin[1] - box.margin[3] - extra);
      forced_height[item.node] = BoxAuto();
    }
    const BoxRect cell{content[0], content[1], area, BoxAuto()};
    if (!Place(item.node, cell, false, content[0], content[1], positioned, depth + 1)) return false;
    outer[n] = box.border[3] + box.margin[0] + box.margin[2];
  }
  for (auto& t : rows) { t.base = t.min_auto ? 0 : t.min; t.limit = t.max_auto || t.fr ? 0 : std::max(t.max, t.base); }
  for (size_t n = 0; n < items.size(); ++n)
    if (items[n].row_span == 1) {
      auto& t = rows[items[n].row];
      if (t.min_auto || t.fr) t.base = std::max(t.base, outer[n]);
      if (t.max_auto || t.fr) t.limit = std::max(t.limit, outer[n]);
    }
  for (auto& t : rows) if (t.fr && !definite) { t.fr = 0; t.max_auto = true; }
  Size(rows, definite ? content[3] : BoxAuto(), row_gap, definite && normal(style.Get("align-content")));
  std::vector<double> column_at(columns.size()), row_at(rows.size());
  for (size_t i = 0; i < columns.size(); ++i) column_at[i] = Sum(columns, 0, i, column_gap) + (i ? column_gap : 0);
  for (size_t i = 0; i < rows.size(); ++i) row_at[i] = Sum(rows, 0, i, row_gap) + (i ? row_gap : 0);
  const auto align_items = style.Get("align-items");
  for (size_t n = 0; n < items.size(); ++n) {
    const auto& item = items[n];
    auto& box = boxes[item.node];
    const double area_height = Sum(rows, item.row, item.row_span, row_gap);
    auto align = nodes[item.node].text ? std::string("start") : nodes[item.node].style.Get("align-self");
    if (align.empty() || align == "auto") align = align_items;
    double offset = 0;
    const auto specified_height = nodes[item.node].text ? std::string() : nodes[item.node].style.Get("height");
    if (!nodes[item.node].text && normal(align) && (specified_height.empty() || specified_height == "auto")) {
      const double extra = box.padding[0] + box.padding[2] + box.edge[0] + box.edge[2];
      const double target = BoxLimit(nodes[item.node].style, false, area_height,
                                     std::max(0.0, area_height - box.margin[0] - box.margin[2] - extra), extra);
      if (std::abs(target - box.content[3]) > 1e-6) {
        if (Relayout(item.node, true)) {
          forced_height[item.node] = target;
          const BoxRect cell{content[0], content[1], Sum(columns, item.column, item.column_span, column_gap), area_height};
          if (!Place(item.node, cell, true, content[0], content[1], positioned, depth + 1)) return false;
        } else {
          Resize(item.node, true, target);
        }
      }
    } else if (align == "center") offset = (area_height - outer[n]) / 2;
    else if (align == "end" || align == "flex-end" || align == "self-end") offset = area_height - outer[n];
    const double x = content[0] + column_at[item.column] + box.margin[3];
    const double y = content[1] + row_at[item.row] + box.margin[0] + std::max(0.0, offset);
    ShiftBoxes(item.node, BoxRound(x) - box.border[0], BoxRound(y) - box.border[1]);
  }
  *height = BoxRound(rows.empty() ? 0 : Sum(rows, 0, rows.size(), row_gap));
  *width = BoxRound(Sum(columns, 0, columns.size(), column_gap));
  return true;
}
}  // namespace ruyijs::domain::styling
