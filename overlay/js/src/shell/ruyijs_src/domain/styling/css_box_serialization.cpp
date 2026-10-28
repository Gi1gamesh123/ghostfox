#include "domain/styling/css_box_value.h"
#include "domain/styling/css_flex_value.h"
#include "domain/styling/css_overflow_value.h"
#include <algorithm>
namespace ruyijs::domain::styling {
std::string CssBoxSerialize(const std::vector<CssDeclarationValue>& rows,
                            const std::string& name, bool* important) {
  if (CssFlexShorthand(name)) return CssFlexSerialize(rows, name, important);
  if (CssOverflowShorthand(name)) return CssOverflowSerialize(rows, important);
  const auto names = CssBoxLonghands(name);
  if (important) *important = false;
  if (names.empty()) return {};
  auto pending = std::find_if(rows.begin(),rows.end(),[&](const auto& row) { return row.name == name; });
  if (pending != rows.end() && std::none_of(rows.begin(),rows.end(),[&](const auto& row) {
    return std::find(names.begin(),names.end(),row.name) != names.end();
  })) {
    if (important) *important = pending->important;
    return pending->value;
  }
  std::vector<std::string> values;
  bool flag = false;
  for (const auto& key : names) {
    auto entry = std::find_if(rows.begin(), rows.end(), [&](const auto& row) { return row.name == key; });
    if (entry == rows.end() || (!values.empty() && entry->important != flag)) return {};
    flag = entry->important; values.push_back(entry->value);
  }
  std::string result;
  const auto wide = [](const auto& v) { return v == "inherit" || v == "initial" ||
      v == "unset" || v == "revert" || v == "revert-layer"; };
  if (std::any_of(values.begin(),values.end(),wide) &&
      !std::all_of(values.begin(),values.end(),[&](const auto& v) { return v == values[0]; })) return {};
  if (std::all_of(values.begin(), values.end(), [&](const auto& v) { return v == values[0]; }) &&
      (values[0] == "inherit" || values[0] == "initial" || values[0] == "unset" ||
       values[0] == "revert" || values[0] == "revert-layer")) result = values[0];
  else if (names.size() == 4) {
    size_t count = 4;
    if (values[3] == values[1]) { count = 3; if (values[2] == values[0]) {
      count = 2; if (values[1] == values[0]) count = 1;
    } }
    for (size_t i = 0; i < count; ++i) result += (i ? " " : "") + values[i];
  } else {
    if (name == "border") {
      for (size_t i = 3; i < 12; ++i) if (values[i] != values[i%3]) return {};
      constexpr const char* defaults[] = {"none", "100%", "1", "0", "stretch"};
      for (size_t i = 12; i < values.size(); ++i) if (values[i] != defaults[i-12]) return {};
    }
    constexpr const char* defaults[] = {"medium", "none", "currentcolor"};
    for (size_t i = 0; i < 3; ++i) if (values[i] != defaults[i])
      result += (result.empty() ? "" : " ")+values[i];
    if (result.empty()) result = "medium";
  }
  if (important) *important = flag;
  return result;
}
}
