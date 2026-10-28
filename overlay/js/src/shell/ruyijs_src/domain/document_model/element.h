#pragma once
#include "domain/document_model/character_data.h"
#include "domain/document_model/textarea_state.h"
#include "domain/document_model/template_content.h"
#include "domain/document_model/input_check_state.h"
#include "domain/document_model/script_state.h"
#include <map>
#include "domain/document_model/svg_view_box_state.h"
#include "domain/document_model/svg_points_state.h"
#include "domain/geometry/svg_length_value.h"

namespace ruyijs::domain::document_model {
inline DomString escape_markup(DomStringView value, bool attribute) {
  DomString out;
  for (char16_t c : value) {
    if (c == u'&') out += u"&amp;";
    else if (c == u'"' && attribute) out += u"&quot;";
    else if (c == u'<' && !attribute) out += u"&lt;";
    else if (c == u'>' && !attribute) out += u"&gt;";
    else if (c == 0xa0) out += u"&nbsp;";
    else out += c;
  }
  return out;
}
class Element : public Node {
 public:
  Element(NodeId id, TagName tag) : Node(id, NodeType::Element), tag_name_(std::move(tag)) {}
  DomString node_name16() const override { return tag_name_.value16(); }
  const TagName& tag_name() const noexcept { return tag_name_; }
  bool has_svg_view_box() const noexcept;
  const SvgViewBoxState& svg_view_box() const noexcept { return svg_view_box_; }
  void preserve_svg_view_box(const SvgViewBoxState& value) noexcept { svg_view_box_ = value; }
  void commit_svg_view_box(SvgViewBoxState value, DomString text);
  // Live polygon/polyline points (element_svg_points.h): shared point records
  // synchronized with the `points` attribute.
  bool has_svg_points() const noexcept;
  const SvgPointsState& svg_points() const noexcept { return svg_points_; }
  SvgPointsState& svg_points() noexcept { return svg_points_; }
  void preserve_svg_points(const SvgPointsState& value) { svg_points_ = value; }
  void commit_svg_points(SvgPointsState state, DomString text);
  // Typed SVG length values committed through SVGLength (element_svg_points.h);
  // a string attribute write drops the typed value, which then reparses.
  const geometry::SvgLengthValue* svg_length(DomStringView local) const noexcept;
  void commit_svg_length(DomStringView local, geometry::SvgLengthValue value, DomString text);
  void preserve_svg_lengths(const Element& other) { svg_lengths_ = other.svg_lengths_; }
  ScriptState& script_state() noexcept { return script_; }
  const ScriptState& script_state() const noexcept { return script_; }
  const std::shared_ptr<const DomString>& is_value() const noexcept { return is_value_; }
  void set_is_value(std::shared_ptr<const DomString> value) noexcept { is_value_ = std::move(value); }
  NodeId shadow_root_id() const noexcept override { return shadow_root_; }
  void set_shadow_root(NodeId id) noexcept { shadow_root_ = id; }
  std::optional<DomString> get_attribute16(DomStringView name) const {
    DomString key = tag_name_.folds_case() ? ascii_lower(name) : DomString(name);
    for (const auto& value : attributes_) if (value.name == key) return value.value;
    return std::nullopt;
  }
  void set_attribute16(DomStringView name, DomStringView value) {
    if (!valid_attribute_local_name(name)) throw DomError(DomErrorCode::InvalidCharacter, "Invalid attribute name");
    DomString key = tag_name_.folds_case() ? ascii_lower(name) : DomString(name);
    DomString replacement(value);
    for (auto& attribute : attributes_)
      if (attribute.namespace_uri.empty() && attribute.name == key) {
        const auto typed = NextSvgViewBox({}, key, replacement, &attribute.value);
        RefreshSvgPoints({}, key, replacement, &attribute.value); ForgetSvgLength({}, key);
        attribute.value.swap(replacement); svg_view_box_ = typed; return;
      }
    const auto typed = NextSvgViewBox({}, key, replacement, nullptr);
    RefreshSvgPoints({}, key, replacement, nullptr); ForgetSvgLength({}, key);
    attributes_.push_back(Attribute{key, std::move(replacement), DomString{}, DomString{}, key});
    svg_view_box_ = typed;
  }
  void remove_attribute16(DomStringView name) {
    DomString key = tag_name_.folds_case() ? ascii_lower(name) : DomString(name);
    auto it = std::find_if(attributes_.begin(), attributes_.end(), [&](const auto& v) { return v.name == key; });
    if (it != attributes_.end()) {
      RemoveSvgViewBox(it->namespace_uri, it->local_name); RemoveSvgPoints(it->namespace_uri, it->local_name);
      ForgetSvgLength(it->namespace_uri, it->local_name); attributes_.erase(it);
    }
  }
  // Namespaced attributes. The qualified name is stored as written (ASCII
  // folded only for the null namespace on an HTML element), so
  // getAttribute(qualified) and serialization keep the source spelling while
  // (namespace, local name) stays the lookup identity, letting two attributes
  // with the same local name in different namespaces coexist.
  std::optional<DomString> get_attribute_ns16(DomStringView namespace_uri, DomStringView local) const {
    for (const auto& attribute : attributes_)
      if (attribute.namespace_uri == namespace_uri && attribute.local_name == local) return attribute.value;
    return std::nullopt;
  }
  bool has_attribute_ns16(DomStringView namespace_uri, DomStringView local) const {
    for (const auto& attribute : attributes_)
      if (attribute.namespace_uri == namespace_uri && attribute.local_name == local) return true;
    return false;
  }
  void set_attribute_ns16(DomStringView namespace_uri, DomStringView qualified, DomStringView value) {
    DomString prefix, local(qualified);
    size_t colon = qualified.find(u':');
    if (colon != DomStringView::npos) {
      prefix = DomString(qualified.substr(0, colon));
      local = DomString(qualified.substr(colon + 1));
      if (prefix.empty()) throw DomError(DomErrorCode::Namespace, "Empty namespace prefix");
      if (namespace_uri.empty()) throw DomError(DomErrorCode::Namespace, "Prefixed attribute requires a namespace");
    }
    if (!valid_attribute_local_name(local)) throw DomError(DomErrorCode::InvalidCharacter, "Invalid attribute name");
    const bool fold = namespace_uri.empty() && tag_name_.folds_case();
    DomString stored_name = fold ? ascii_lower(qualified) : DomString(qualified);
    DomString stored_prefix = fold ? ascii_lower(prefix) : prefix;
    DomString stored_local = fold ? ascii_lower(local) : local;
    DomString ns(namespace_uri), replacement(value);
    for (auto& attribute : attributes_)
      if (attribute.namespace_uri == ns && attribute.local_name == stored_local) {
        const auto typed = NextSvgViewBox(ns, stored_local, replacement, &attribute.value);
        RefreshSvgPoints(ns, stored_local, replacement, &attribute.value); ForgetSvgLength(ns, stored_local);
        attribute.name = std::move(stored_name);
        attribute.prefix = std::move(stored_prefix);
        attribute.value.swap(replacement); svg_view_box_ = typed;
        return;
      }
    const auto typed = NextSvgViewBox(ns, stored_local, replacement, nullptr);
    RefreshSvgPoints(ns, stored_local, replacement, nullptr); ForgetSvgLength(ns, stored_local);
    attributes_.push_back(Attribute{std::move(stored_name), std::move(replacement),
                                    std::move(ns), std::move(stored_prefix), std::move(stored_local)});
    svg_view_box_ = typed;
  }
  void remove_attribute_ns16(DomStringView namespace_uri, DomStringView local) {
    for (auto it = attributes_.begin(); it != attributes_.end(); ++it)
      if (it->namespace_uri == namespace_uri && it->local_name == local) {
        RemoveSvgViewBox(it->namespace_uri, it->local_name); RemoveSvgPoints(it->namespace_uri, it->local_name);
        ForgetSvgLength(it->namespace_uri, it->local_name); attributes_.erase(it); return;
      }
  }
  const std::vector<Attribute>& attributes16() const noexcept { return attributes_; }
  size_t attribute_count() const noexcept { return attributes_.size(); }
  // Public ruyijs control state is independent of attributes and resets on clone.
  bool option_selected() const {
    return selected_override_.value_or(get_attribute16(u"selected").has_value());
  }
  void set_option_selected(bool value) noexcept { selected_override_ = value; }
  InputCheckState& input_check_state() noexcept { return input_check_; }
  const InputCheckState& input_check_state() const noexcept { return input_check_; }
  std::optional<DomString>& input_current_value() noexcept { return input_value_; }
  const std::optional<DomString>& input_current_value() const noexcept { return input_value_; }
  bool input_checked() const {
    return input_check_.checked.value_or(has_attribute_ns16({}, u"checked"));
  }
  const DomString& custom_validity() const noexcept { return custom_validity_; }
  void set_custom_validity(DomString value) { custom_validity_.swap(value); }
  TextareaState& textarea_state() noexcept { return textarea_; }
  const TextareaState& textarea_state() const noexcept { return textarea_; }
  const DomString& dialog_return_value() const noexcept { return dialog_return_value_; }
  void set_dialog_return_value(DomString value) { dialog_return_value_.swap(value); }
  TemplateContentRef template_content() const noexcept { return template_content_; }
  void set_template_content(TemplateContentRef value) noexcept { template_content_ = value; }
  size_t storage_units() const noexcept override {
    size_t count = tag_name_.storage_units() + custom_validity_.size() + textarea_.value.size() + dialog_return_value_.size();
    if (is_value_) count += is_value_->size();
    if (input_value_) count += input_value_->size();
    for (const auto& attr : attributes_) count += attr.name.size() + attr.value.size();
    return count;
  }
  bool has_attribute(const std::string& name) const { return get_attribute16(from_utf8(name)).has_value(); }
  std::string get_attribute(const std::string& name) const { return to_utf8(get_attribute16(from_utf8(name)).value_or(DomString{})); }
  void set_attribute(const std::string& name, const std::string& value) { set_attribute16(from_utf8(name), from_utf8(value)); }
  void remove_attribute(const std::string& name) { remove_attribute16(from_utf8(name)); }
  std::map<std::string, std::string> attributes() const {
    std::map<std::string, std::string> result;
    for (const auto& attr : attributes_) result.emplace(to_utf8(attr.name), to_utf8(attr.value));
    return result;
  }
  std::string element_id() const { return get_attribute("id"); }
  void set_element_id(const std::string& value) { set_attribute("id", value); }
  std::string class_name() const { return get_attribute("class"); }
  void set_class_name(const std::string& value) { set_attribute("class", value); }
  std::vector<DomString> class_list16() const { return tokens(get_attribute16(u"class").value_or(DomString{})); }
  bool has_class16(DomStringView value) const {
    auto all = class_list16(); return std::find(all.begin(), all.end(), value) != all.end();
  }
  std::vector<std::string> class_list() const {
    std::vector<std::string> out; for (const auto& value : class_list16()) out.push_back(to_utf8(value)); return out;
  }
  bool has_class(const std::string& value) const { return has_class16(from_utf8(value)); }
  void add_class(const std::string& value) {
    if (!has_class(value)) { auto all = class_name(); set_class_name(all.empty() ? value : all + " " + value); }
  }
  void remove_class(const std::string& value) {
    auto all = class_list(); all.erase(std::remove(all.begin(), all.end(), value), all.end());
    std::string joined; for (auto& item : all) { if (!joined.empty()) joined += ' '; joined += item; } set_class_name(joined);
  }
  std::string get_data(const std::string& name) const { return get_attribute("data-" + name); }
  void set_data(const std::string& name, const std::string& value) { set_attribute("data-" + name, value); }
  DomString serialize_open_tag16() const {
    DomString out = u"<" + serialized_name16();
    if (is_value_ && !has_attribute_ns16({}, u"is"))
      out += u" is=\"" + escape_markup(*is_value_, true) + u"\"";
    for (const auto& attr : attributes_) out += u" " + attr.name + u"=\"" + escape_markup(attr.value, true) + u"\"";
    return out + u">";
  }
  DomString serialize_close_tag16() const { return tag_name_.is_void() ? DomString{} : u"</" + serialized_name16() + u">"; }
  DomString serialized_name16() const {
    const auto ns = tag_name_.namespace16();
    return ns == u"http://www.w3.org/1999/xhtml" || ns == u"http://www.w3.org/2000/svg" ||
        ns == u"http://www.w3.org/1998/Math/MathML" ? tag_name_.local16() : tag_name_.qualified16();
  }
  std::string serialize_open_tag() const { return to_utf8(serialize_open_tag16()); }
  std::string serialize_close_tag() const { return to_utf8(serialize_close_tag16()); }
  bool is_tag(const std::string& name) const { return tag_name_ == TagName(name); }
 private:
  SvgViewBoxState NextSvgViewBox(DomStringView, DomStringView, DomStringView, const DomString*) const;
  void RemoveSvgViewBox(DomStringView, DomStringView) noexcept;
  SvgViewBoxState svg_view_box_;
  void RefreshSvgPoints(DomStringView, DomStringView, DomStringView, const DomString*);
  void RemoveSvgPoints(DomStringView, DomStringView) noexcept;
  void ForgetSvgLength(DomStringView, DomStringView) noexcept;
  SvgPointsState svg_points_;
  std::vector<std::pair<DomString, geometry::SvgLengthValue>> svg_lengths_;
  TagName tag_name_;
  ScriptState script_;
  std::shared_ptr<const DomString> is_value_;
  NodeId shadow_root_ = 0;
  std::vector<Attribute> attributes_;
  std::optional<bool> selected_override_;
  InputCheckState input_check_;
  std::optional<DomString> input_value_;
  DomString custom_validity_;
  TextareaState textarea_;
  DomString dialog_return_value_;
  TemplateContentRef template_content_;
};
}

#include "domain/document_model/element_svg_view_box.h"
#include "domain/document_model/element_svg_points.h"
