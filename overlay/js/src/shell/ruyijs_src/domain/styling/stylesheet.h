#pragma once

#include "domain/shared/aggregate_root.h"
#include "domain/shared/domain_event.h"
#include "domain/styling/css_rule_value.h"
#include "domain/styling/css_declarations.h"
#include "domain/styling/css_condition_value.h"
#include "domain/styling/css_components.h"
#include "domain/styling/css_selector.h"
#include "domain/styling/css_font_value.h"
#include "domain/styling/css_box_value.h"
#include "domain/styling/css_variables.h"
#include "domain/styling/css_overflow_value.h"

#include <cstdint>
#include <unordered_map>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace ruyijs::domain::styling {

using StyleSheetId = uint64_t;

/// Value Object: A CSS property (name + value + important flag).
struct CssProperty {
    std::string name;
    std::string value;
    bool important = false;

    bool operator==(const CssProperty& o) const {
        return name == o.name && value == o.value && important == o.important;
    }
};

/// Value Object: CSS selector with specificity.
struct CssSelector {
    std::string text;       // e.g., "div.container > p"
    int specificity_a = 0;  // ID selectors count
    int specificity_b = 0;  // class/attribute/pseudo-class count
    int specificity_c = 0;  // element/pseudo-element count

    /// Compare specificity (higher wins)
    bool operator<(const CssSelector& o) const {
        if (specificity_a != o.specificity_a) return specificity_a < o.specificity_a;
        if (specificity_b != o.specificity_b) return specificity_b < o.specificity_b;
        return specificity_c < o.specificity_c;
    }
};

/// Entity: A CSS rule (style rule, media rule, keyframes, etc.)
class CssRule {
public:
    enum class Type {
        Style = 1,
        Charset = 2,
        Import = 3,
        Media = 4,
        FontFace = 5,
        Page = 6,
        Keyframes = 7,
        Keyframe = 8,
        Supports = 12,
        Namespace = 10,
        Layer = 15,
    };

    CssRule(Type type, CssSelector selector = {})
        : type_(type), selector_(std::move(selector)) {}

    Type type() const noexcept { return type_; }
    const CssSelector& selector() const noexcept { return selector_; }

    // Style rule properties
    const std::vector<CssProperty>& properties() const noexcept { return properties_; }

    void set_property(const std::string& name, const std::string& value, bool important = false) {
        for (auto& prop : properties_) {
            if (prop.name == name) {
                prop.value = value;
                prop.important = important;
                return;
            }
        }
        properties_.push_back({name, value, important});
    }

    std::string get_property_value(const std::string& name) const {
        for (const auto& prop : properties_) {
            if (prop.name == name) return prop.value;
        }
        return "";
    }

    void remove_property(const std::string& name) {
        properties_.erase(
            std::remove_if(properties_.begin(), properties_.end(),
                [&name](const CssProperty& p) { return p.name == name; }),
            properties_.end());
    }

    /// cssText serialization
    std::string css_text() const {
        std::string result = selector_.text + " { ";
        for (const auto& prop : properties_) {
            result += prop.name + ": " + prop.value;
            if (prop.important) result += " !important";
            result += "; ";
        }
        result += "}";
        return result;
    }

private:
    Type type_;
    CssSelector selector_;
    std::vector<CssProperty> properties_;
};

/// Entity: CSSStyleDeclaration (inline style or computed style).
class StyleDeclaration {
public:
    StyleDeclaration() = default;
    // Computed-value metadata only; inherited rem units use the original root.
    double root_font_size = 16;
    double computed_font_size = 16;
    double computed_line_height = 0;
    bool font_computed = false, line_height_computed = false;
    unsigned specified_margin_mask = 0, specified_border_mask = 0, specified_padding_mask = 0;

    using Properties = std::map<std::string, CssProperty>;
    static bool custom(const std::string& name) { return name.size() > 2 && name.compare(0, 2, "--") == 0; }

    void set_property(const std::string& name, const std::string& value, bool important = false) {
        if (custom(name)) own_customs()[name] = {name, value, important};
        else properties_[name] = {name, value, important};
    }

    bool has_property(const std::string& name) const {
        if (!custom(name)) return properties_.count(name) != 0;
        return customs_ && customs_->count(name);
    }

    std::string get_property_value(const std::string& name) const {
        const auto* table = custom(name) ? customs_.get() : &properties_;
        if (!table) return "";
        auto it = table->find(name);
        return (it != table->end()) ? it->second.value : "";
    }

    bool get_property_priority(const std::string& name) const {
        const auto* table = custom(name) ? customs_.get() : &properties_;
        if (!table) return false;
        auto it = table->find(name);
        return (it != table->end()) ? it->second.important : false;
    }

    void remove_property(const std::string& name) {
        if (!custom(name)) { properties_.erase(name); return; }
        if (has_property(name)) own_customs().erase(name);
    }

    size_t length() const noexcept { return properties_.size() + (customs_ ? customs_->size() : 0); }

    std::string item(size_t index) const {
        size_t i = 0;
        for (const auto& [name, _] : properties_) {
            if (i == index) return name;
            ++i;
        }
        if (customs_) for (const auto& [name, _] : *customs_) {
            if (i == index) return name;
            ++i;
        }
        return "";
    }

    // Custom properties (CSS Variables 1 §2): shared with the parent until
    // this element declares or resolves one of its own (copy on write).
    const Properties& customs() const noexcept {
        static const Properties empty;
        return customs_ ? *customs_ : empty;
    }
    void inherit_customs(const StyleDeclaration& parent) { customs_ = parent.customs_; }
    // Identity of the shared custom-property map (null when none): elements that
    // inherit the same map share it, so a serialization can be memoized by it.
    const void* customs_identity() const noexcept { return customs_.get(); }

    /// cssText: "color: red; font-size: 14px;"
    std::string css_text() const {
        std::string result;
        for (const auto& [_, prop] : properties_) {
            result += prop.name + ": " + prop.value;
            if (prop.important) result += " !important";
            result += "; ";
        }
        return result;
    }

    void set_css_text(const std::string& text) {
        properties_.clear();
        customs_.reset();
        for (const auto& value : CssDeclarations(text)) set_property(value.name, value.value, value.important);
    }

    // The standard properties; custom properties are customs().
    const Properties& all() const noexcept { return properties_; }

private:
    static std::string trim(const std::string& s) {
        auto start = s.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        auto end = s.find_last_not_of(" \t\n\r");
        return s.substr(start, end - start + 1);
    }

    Properties& own_customs() {
        if (!customs_) customs_ = std::make_shared<Properties>();
        else if (customs_.use_count() > 1) customs_ = std::make_shared<Properties>(*customs_);
        return *customs_;
    }

    std::map<std::string, CssProperty> properties_;
    std::shared_ptr<Properties> customs_;
};

/// Aggregate Root: CSSStyleSheet
class StyleSheetAggregate : public shared::AggregateRoot<StyleSheetId> {
public:
    explicit StyleSheetAggregate(StyleSheetId id, std::string href = "")
        : AggregateRoot<StyleSheetId>(id), href_(std::move(href)) {}

    const std::string& href() const noexcept { return href_; }
    bool disabled() const noexcept { return disabled_; }
    void set_disabled(bool d) noexcept { disabled_ = d; }

    const std::vector<CssRule>& rules() const noexcept { return rules_; }
    size_t rule_count() const noexcept { return rules_.size(); }

    void add_rule(CssRule rule) { rules_.push_back(std::move(rule)); }

    void remove_rule(size_t index) {
        if (index < rules_.size()) {
            rules_.erase(rules_.begin() + static_cast<ptrdiff_t>(index));
        }
    }

    /// Project the canonical Styling grammar into this transient cascade input.
    void parse_css(const std::string& css) {
        auto parsed = ParseCssRuleValues(css);
        append_rule_values(parsed.rules, {});
    }
    void append_rule_values(const std::vector<std::shared_ptr<CssomRuleValue>>&,
                            const CssMediaEnvironment&);

private:
    static std::string trim_str(const std::string& s) {
        auto start = s.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        auto end = s.find_last_not_of(" \t\n\r");
        return s.substr(start, end - start + 1);
    }

    std::string href_;
    bool disabled_ = false;
    std::vector<CssRule> rules_;
};

// ---------------------------------------------------------------------------
// Selector matching, the cascade, inheritance and computed values. The donor
// service matched a single simple selector only; a real cascade needs compound
// and descendant selectors, the (a,b,c) specificity triple, inheritance and the
// specification initial values. This extends the same owner in place so there
// is exactly one cascade service, not a second engine beside it.
// ---------------------------------------------------------------------------

namespace detail {
inline std::string TrimAscii(const std::string& text) {
    size_t begin = 0, end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return text.substr(begin, end - begin);
}
inline std::string LowerAscii(const std::string& text) {
    std::string out = text;
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}
inline bool EqualsIgnoreCase(const std::string& a, const std::string& b) {
    return LowerAscii(a) == LowerAscii(b);
}
inline bool IdentStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '-';
}
inline bool IdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-';
}
}  // namespace detail

/// Domain Service: Style resolution (cascade, inheritance, computed values).
// One sheet's style rules bucketed by their selectors' IndexKey, as (rule
// position, selector position) in source order, with the parsed selectors.
// It depends only on the sheet, so contexts over the same unchanged sheets
// may share one store (RuleIndexStore) for its lifetime.
struct RuleIndex {
    using Entries = std::vector<std::pair<uint32_t, uint32_t>>;
    std::vector<const std::vector<ParsedSelector>*> parsed;
    Entries universal;
    std::unordered_map<std::string, Entries> ids, classes, types;
};
struct RuleIndexStore {
    std::map<std::string, std::vector<ParsedSelector>> selectors;
    std::map<const StyleSheetAggregate*, RuleIndex> indexes;
};
struct StyleResolutionContext {
    std::map<std::pair<uint64_t,uint64_t>, StyleDeclaration> computed;
    using Entries = RuleIndex::Entries;
    // Built once per sheet for the store's lifetime, during which the sheets
    // do not change. A context owns a store unless one is shared with it.
    std::shared_ptr<RuleIndexStore> store = std::make_shared<RuleIndexStore>();
    const RuleIndex& Index(const StyleSheetAggregate& sheet) {
        auto& indexes = store->indexes;
        auto& selectors = store->selectors;
        auto found = indexes.find(&sheet);
        if (found != indexes.end()) return found->second;
        RuleIndex& index = indexes[&sheet];
        const auto& rules = sheet.rules();
        index.parsed.assign(rules.size(), nullptr);
        for (uint32_t r = 0; r < rules.size(); ++r) {
            if (rules[r].type() != CssRule::Type::Style) continue;
            const std::string& text = rules[r].selector().text;
            auto parsed = selectors.find(text);
            if (parsed == selectors.end())
                parsed = selectors.emplace(text, ParseSelectorList(text)).first;
            index.parsed[r] = &parsed->second;
            for (uint32_t s = 0; s < parsed->second.size(); ++s) {
                char kind = 0;
                std::string key;
                parsed->second[s].IndexKey(&kind, &key);
                Entries& bucket = kind == '#' ? index.ids[key] : kind == '.' ? index.classes[key] :
                                  kind == 't' ? index.types[key] : index.universal;
                bucket.emplace_back(r, s);
            }
        }
        return index;
    }
};
class StyleResolutionService {
public:
    /// The donor entry point, kept so callers with only tag/id/classes still
    /// resolve through the same cascade. It is a root element with no parent.
    static StyleDeclaration compute_style(
        const std::vector<const StyleSheetAggregate*>& sheets,
        const std::string& element_tag, const std::string& element_id,
        const std::vector<std::string>& element_classes)
    {
        ElementNode element;
        element.tag = element_tag;
        element.id = element_id;
        element.classes = element_classes;
        return compute_style(sheets, element);
    }

    /// The cascade for one element: user-agent defaults, then author rules by
    /// (importance, inline origin, specificity, source order), then the inline
    /// declaration, then inheritance of the inherited properties.
    static StyleDeclaration compute_style(
        const std::vector<const StyleSheetAggregate*>& sheets, const ElementNode& element,
        StyleResolutionContext* shared = nullptr)
    {
        StyleResolutionContext local;
        if (!shared) shared = &local;
        const auto identity = std::make_pair(element.document, element.key);
        if (element.key) {
            const auto previous = shared->computed.find(identity);
            if (previous != shared->computed.end()) return previous->second;
        }
        std::map<std::string, Candidate> winners;
        uint64_t order = 0;
        std::string display = user_agent_display(element.tag);
        if (!display.empty()) {
            Candidate candidate;
            candidate.value = display;
            candidate.order = order++;
            winners["display"] = candidate;
        }
        for (const auto* sheet : sheets) {
            if (!sheet || sheet->disabled()) continue;
            // Only the buckets this element can match, visited in source
            // order, so cascade order is that of the full rule walk.
            const auto& index = shared->Index(*sheet);
            StyleResolutionContext::Entries candidates(index.universal);
            auto add = [&](const std::unordered_map<std::string, StyleResolutionContext::Entries>& map,
                           const std::string& key) {
                const auto bucket = map.find(key);
                if (bucket != map.end())
                    candidates.insert(candidates.end(), bucket->second.begin(), bucket->second.end());
            };
            if (!element.id.empty()) add(index.ids, element.id);
            for (const auto& name : element.classes) add(index.classes, name);
            add(index.types, detail::LowerAscii(element.tag));
            std::sort(candidates.begin(), candidates.end());
            candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
            const auto& rules = sheet->rules();
            for (const auto& entry : candidates) {
                const auto& rule = rules[entry.first];
                {
                    const ParsedSelector& selector = (*index.parsed[entry.first])[entry.second];
                    if (!selector.matches(element)) continue;
                    for (const auto& prop : rule.properties()) {
                        Candidate candidate;
                        candidate.value = prop.value;
                        candidate.important = prop.important;
                        candidate.a = selector.specificity_a();
                        candidate.b = selector.specificity_b();
                        candidate.c = selector.specificity_c();
                        candidate.order = order++;
                        Consider(&winners, prop.name, candidate);
                    }
                }
            }
        }
        if (!element.inline_style.empty()) {
            for (const auto& entry : CssDeclarations(element.inline_style)) {
                Candidate candidate;
                candidate.value = entry.value;
                candidate.important = entry.important;
                candidate.inline_origin = true;
                candidate.order = order++;
                Consider(&winners, entry.name, candidate);
            }
        }
        auto parent = element.related('>');
        bool has_parent = parent != nullptr;
        StyleDeclaration parent_style;
        if (has_parent) parent_style = compute_style(sheets, *parent, shared);
        StyleDeclaration result;
        result.computed_font_size = parent_style.computed_font_size;
        result.computed_line_height = parent_style.computed_line_height;
        result.font_computed = parent_style.font_computed;
        result.line_height_computed = parent_style.line_height_computed;
        if (has_parent) result.inherit_customs(parent_style);
        for (const auto& entry : winners) {
            const std::string& name = entry.first;
            std::string value = detail::TrimAscii(entry.second.value);
            std::string keyword = detail::LowerAscii(value);
            const bool inherits = keyword == "inherit" || keyword == "unset" ||
                                  keyword == "revert" || keyword == "revert-layer";
            if (!inherits) {
                if (name == "font-size") result.font_computed = false;
                if (name == "line-height") result.line_height_computed = false;
            }
            if (keyword == "inherit" || keyword == "unset" || keyword == "revert" || keyword == "revert-layer") {
                bool from_parent = keyword == "inherit" || is_inherited(name);
                if (from_parent && has_parent && parent_style.has_property(name))
                    result.set_property(name, parent_style.get_property_value(name), false);
                else if (StyleDeclaration::custom(name))
                    result.remove_property(name);  // the guaranteed-invalid value
                else
                    result.set_property(name, initial_value(name), false);
            } else if (keyword == "initial") {
                if (StyleDeclaration::custom(name)) result.remove_property(name);
                else result.set_property(name, initial_value(name), false);
            } else {
                result.set_property(name, value, entry.second.important);
            }
        }
        for (const auto& entry : parent_style.all()) {
            if (!is_inherited(entry.first)) continue;
            if (result.all().count(entry.first)) continue;
            result.set_property(entry.first, entry.second.value, entry.second.important);
        }
        std::map<std::string, uint64_t> declared;
        for (const auto& entry : winners) declared[entry.first] = entry.second.order;
        CssResolveVariables(result, declared,
                            {[](const std::string& name) { return is_inherited(name); },
                             [](const std::string& name) { return initial_value(name); },
                             has_parent ? &parent_style : nullptr});
        CssComputeFont(result, parent_style, parent_style.root_font_size);
        result.root_font_size = has_parent ? parent_style.root_font_size : result.computed_font_size;
        unsigned side_index = 0;
        for (const auto* side : {"top", "right", "bottom", "left"}) {
            if (winners.count(std::string("margin-") + side)) result.specified_margin_mask |= 1u << side_index;
            if (winners.count(std::string("border-") + side + "-style")) result.specified_border_mask |= 1u << side_index;
            if (winners.count(std::string("padding-") + side)) result.specified_padding_mask |= 1u << side_index;
            ++side_index;
        }
        CssComputeBox(result, result.root_font_size);
        CssComputeOverflow(result);
        if (element.key) shared->computed.emplace(identity, result);
        return result;
    }

    /// The specification initial value of a property, or "" when unknown.
    static std::string initial_value(const std::string& property) {
        if (property == "font-family") return CssInitialFontFamily();  // set from the browser locale
        const auto& table = InitialValues();
        auto it = table.find(property);
        return it == table.end() ? std::string() : it->second;
    }

    /// The known property names and their specification initial values, so a
    /// computed style can answer an undeclared property without a second table.
    static const std::map<std::string, std::string>& initial_values() {
        auto& table = const_cast<std::map<std::string, std::string>&>(InitialValues());
        table["font-family"] = CssInitialFontFamily();  // follows the browser locale
        return table;
    }

    /// Whether an undeclared property inherits its parent's computed value.
    static bool is_inherited(const std::string& property) {
        // Custom properties are inherited (CSS Variables 1 §2).
        if (property.size() > 2 && property.compare(0, 2, "--") == 0) return true;
        for (const auto& font : CssFontDefaults()) if (font.name == property) return true;
        static const std::vector<std::string> names = {
            "color", "font-family", "font-size", "font-style", "font-variant",
            "font-weight", "line-height", "letter-spacing", "word-spacing", "text-align",
            "text-indent", "text-transform", "white-space", "visibility", "cursor",
            "direction", "list-style", "list-style-type", "list-style-position",
            "list-style-image", "quotes", "border-collapse", "border-spacing",
            "caption-side", "empty-cells", "orphans", "widows"};
        return std::find(names.begin(), names.end(), property) != names.end();
    }

    /// The user-agent default display keyword for an HTML tag. This is the UA
    /// stylesheet's contribution, not the property's initial value ("inline").
    static std::string user_agent_display(const std::string& tag) {
        std::string name = detail::LowerAscii(tag);
        static const std::vector<std::string> block = {
            "html", "body", "div", "p", "h1", "h2", "h3", "h4", "h5", "h6",
            "ul", "ol", "li", "dl", "dt", "dd", "blockquote", "pre", "hr",
            "form", "fieldset", "header", "footer", "section", "article",
            "aside", "nav", "main", "figure", "figcaption", "address", "center"};
        static const std::vector<std::string> none = {
            "head", "title", "meta", "link", "style", "script", "base", "template",
            "param", "source", "track", "col", "colgroup"};
        static const std::vector<std::string> inline_block = {
            "input", "button", "select", "textarea", "img", "canvas", "video",
            "audio", "object", "embed"};
        if (std::find(block.begin(), block.end(), name) != block.end()) return "block";
        if (std::find(none.begin(), none.end(), name) != none.end()) return "none";
        if (std::find(inline_block.begin(), inline_block.end(), name) != inline_block.end())
            return "inline-block";
        if (name == "li") return "list-item";
        if (name == "table") return "table";
        if (name == "tr") return "table-row";
        if (name == "td" || name == "th") return "table-cell";
        if (name == "tbody" || name == "thead" || name == "tfoot") return "table-row-group";
        if (name == "caption") return "table-caption";
        return "inline";
    }

private:
    struct Candidate {
        std::string value;
        bool important = false;
        bool inline_origin = false;
        int a = 0, b = 0, c = 0;
        uint64_t order = 0;
    };

    static bool Beats(const Candidate& candidate, const Candidate& current) {
        if (candidate.important != current.important) return candidate.important;
        if (candidate.inline_origin != current.inline_origin) return candidate.inline_origin;
        if (candidate.a != current.a) return candidate.a > current.a;
        if (candidate.b != current.b) return candidate.b > current.b;
        if (candidate.c != current.c) return candidate.c > current.c;
        return candidate.order >= current.order;
    }

    static void Consider(std::map<std::string, Candidate>* winners, const std::string& name,
                         const Candidate& candidate) {
        auto it = winners->find(name);
        if (it == winners->end() || Beats(candidate, it->second)) (*winners)[name] = candidate;
    }

    static const std::map<std::string, std::string>& InitialValues() {
        static const std::map<std::string, std::string> table = [] {
          std::map<std::string, std::string> values = {
            {"display", "inline"}, {"color", "rgb(0, 0, 0)"},
            {"background-color", "rgba(0, 0, 0, 0)"}, {"background-image", "none"},
            {"width", "auto"}, {"height", "auto"}, {"min-width", "auto"}, {"min-height", "auto"},
            {"max-width", "none"}, {"max-height", "none"},
            {"box-sizing", "content-box"}, {"margin-top", "0px"}, {"margin-right", "0px"},
            {"margin-bottom", "0px"}, {"margin-left", "0px"},
            {"padding-top", "0px"}, {"padding-right", "0px"},
            {"padding-bottom", "0px"}, {"padding-left", "0px"},
            {"border-top-width", "medium"}, {"border-right-width", "medium"},
            {"border-bottom-width", "medium"}, {"border-left-width", "medium"},
            {"border-top-style", "none"}, {"border-right-style", "none"},
            {"border-bottom-style", "none"}, {"border-left-style", "none"},
            {"border-top-color", "currentcolor"}, {"border-right-color", "currentcolor"},
            {"border-bottom-color", "currentcolor"}, {"border-left-color", "currentcolor"},
            {"position", "static"}, {"visibility", "visible"}, {"opacity", "1"},
            {"content-visibility", "visible"}, {"transform", "none"},
            {"overflow-x", "visible"}, {"overflow-y", "visible"}, {"font-size", "16px"}, {"font-style", "normal"},
            {"font-weight", "400"}, {"font-family", CssInitialFontFamily()}, {"line-height", "normal"},
            {"text-align", "start"}, {"text-decoration", "none solid rgb(0, 0, 0)"},
            {"white-space", "normal"}, {"letter-spacing", "normal"}, {"word-spacing", "0px"},
            {"cursor", "auto"}, {"float", "none"}, {"clear", "none"}, {"z-index", "auto"},
            {"left", "auto"}, {"top", "auto"}, {"right", "auto"}, {"bottom", "auto"},
            {"flex-grow", "0"}, {"flex-shrink", "1"}, {"flex-basis", "auto"},
            {"flex-direction", "row"}, {"flex-wrap", "nowrap"}, {"order", "0"},
            {"align-items", "normal"}, {"align-self", "auto"}, {"align-content", "normal"},
            {"justify-content", "normal"}, {"row-gap", "normal"}, {"column-gap", "normal"}};
          for (const auto& font : CssFontDefaults()) values.try_emplace(font.name, font.value);
          return values;
        }();
        return table;
    }
};
} // namespace ruyijs::domain::styling
