// The HTML Sanitizer configuration model (Firefox 158 dom/security/sanitizer/
// Sanitizer.cpp: CanonicalizeConfiguration, IsValid, Get, the allow/remove/replace
// operations and removeUnsafe), without any engine dependency. Names are UTF-8;
// an empty namespace stands for null. Element and attribute sets keep insertion
// order like Firefox's hash sets do not matter, since get() sorts every list.
#ifndef RUYIJS_DOMAIN_DOCUMENT_MODEL_SANITIZER_CONFIG_H
#define RUYIJS_DOMAIN_DOCUMENT_MODEL_SANITIZER_CONFIG_H
#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ruyijs::domain::document_model::sanitizer {
inline constexpr const char* kHtml = "http://www.w3.org/1999/xhtml";
inline constexpr const char* kSvg = "http://www.w3.org/2000/svg";
inline constexpr const char* kMathml = "http://www.w3.org/1998/Math/MathML";

struct Name {
  std::string name, ns;  // ns "" = null
  bool nullNs = true;
  bool operator==(const Name& o) const { return name == o.name && nullNs == o.nullNs && ns == o.ns; }
  // https://wicg.github.io/sanitizer-api/#sanitizerconfig-less-than-item
  bool operator<(const Name& o) const {
    if (nullNs != o.nullNs) return nullNs;
    if (!nullNs && ns != o.ns) return ns < o.ns;
    return name < o.name;
  }
  bool IsData() const {  // a custom data attribute: null namespace, "data-" prefix
    return nullNs && name.size() > 5 && name.compare(0, 5, "data-") == 0;
  }
  std::string Display() const {
    return "\"" + name + "\" (namespace: " + (nullNs ? std::string("null") : "\"" + ns + "\"") + ")";
  }
};
inline Name Make(std::string name, const char* ns) {
  Name n{std::move(name), ns ? ns : "", ns == nullptr || !*ns};
  return n;
}
using Set = std::vector<Name>;
inline bool Has(const Set& s, const Name& n) { return std::find(s.begin(), s.end(), n) != s.end(); }
inline bool Insert(Set& s, const Name& n) { if (Has(s, n)) return false; s.push_back(n); return true; }
inline bool Erase(Set& s, const Name& n) {
  auto it = std::find(s.begin(), s.end(), n);
  if (it == s.end()) return false;
  s.erase(it);
  return true;
}
struct ElementAttributes {
  std::optional<Set> attributes, removeAttributes;
  bool operator==(const ElementAttributes& o) const {
    auto same = [](const std::optional<Set>& a, const std::optional<Set>& b) {
      if (a.has_value() != b.has_value()) return false;
      if (!a) return true;
      Set x = *a, y = *b;
      std::sort(x.begin(), x.end());
      std::sort(y.begin(), y.end());
      return x == y;
    };
    return same(attributes, o.attributes) && same(removeAttributes, o.removeAttributes);
  }
};
using ElementMap = std::vector<std::pair<Name, ElementAttributes>>;
inline ElementAttributes* Find(ElementMap& m, const Name& n) {
  for (auto& e : m) if (e.first == n) return &e.second;
  return nullptr;
}

// One configuration value as the WebIDL dictionary delivered it ("WasPassed").
struct Input {
  struct Element { Name name; std::optional<std::vector<Name>> attributes, removeAttributes; };
  std::optional<std::vector<Element>> elements;
  std::optional<std::vector<Name>> removeElements, replaceWithChildrenElements, attributes, removeAttributes;
  std::optional<std::vector<std::string>> processingInstructions, removeProcessingInstructions;
  std::optional<bool> comments, dataAttributes, javascriptURLs;
};

class Config {
 public:
  std::optional<ElementMap> elements;
  std::optional<Set> removeElements, replaceWithChildrenElements, attributes, removeAttributes;
  std::optional<std::vector<std::string>> processingInstructions, removeProcessingInstructions;
  bool comments = false, javascriptURLs = false;
  std::optional<bool> dataAttributes;

  // https://html.spec.whatwg.org/#canonicalize-the-configuration then IsValid; the
  // returned text is the TypeError message (empty on success).
  std::string Set(const Input& in, bool permissive) {
    if (!in.elements && !in.removeElements) removeElements.emplace();
    if (!in.attributes && !in.removeAttributes) removeAttributes.emplace();
    if (!in.processingInstructions && !in.removeProcessingInstructions) {
      if (permissive) removeProcessingInstructions.emplace(); else processingInstructions.emplace();
    }
    if (in.elements) {
      ElementMap map;
      for (const auto& element : *in.elements) {
        for (const auto& e : map)
          if (e.first == element.name) return "Duplicate element " + element.name.Display() + " in 'elements'.";
        ElementAttributes attrs;
        std::string error = CanonicalAttributes(element, &attrs, true);
        if (!error.empty()) return error;
        map.emplace_back(element.name, std::move(attrs));
      }
      elements = std::move(map);
    }
    std::string error;
    auto list = [&](const std::optional<std::vector<Name>>& source, std::optional<sanitizer::Set>* target,
                    const char* label, const char* kind) {
      if (!source || !error.empty()) return;
      sanitizer::Set s;
      for (const Name& n : *source)
        if (!Insert(s, n)) { error = std::string("Duplicate ") + kind + " " + n.Display() + " in '" + label + "'."; return; }
      *target = std::move(s);
    };
    list(in.removeElements, &removeElements, "removeElements", "element");
    list(in.attributes, &attributes, "attributes", "attribute");
    list(in.removeAttributes, &removeAttributes, "removeAttributes", "attribute");
    list(in.replaceWithChildrenElements, &replaceWithChildrenElements, "replaceWithChildrenElements", "element");
    if (!error.empty()) return error;
    auto pis = [&](const std::optional<std::vector<std::string>>& source, std::optional<std::vector<std::string>>* target,
                   const char* label) {
      if (!source || !error.empty()) return;
      std::vector<std::string> s;
      for (const std::string& t : *source) {
        if (std::find(s.begin(), s.end(), t) != s.end()) {
          error = "Duplicate processing instruction \"" + t + "\" in '" + label + "'.";
          return;
        }
        s.push_back(t);
      }
      *target = std::move(s);
    };
    pis(in.processingInstructions, &processingInstructions, "processingInstructions");
    pis(in.removeProcessingInstructions, &removeProcessingInstructions, "removeProcessingInstructions");
    if (!error.empty()) return error;
    comments = in.comments ? *in.comments : permissive;
    if (in.dataAttributes) dataAttributes = *in.dataAttributes;
    else if (in.attributes) dataAttributes = permissive;
    javascriptURLs = in.javascriptURLs ? *in.javascriptURLs : permissive;
    return Valid();
  }
  std::string Valid() const;
  bool AllowElement(const Input::Element& element);
  bool RemoveElement(const Name& element);
  bool ReplaceElementWithChildren(const Name& element);
  bool AllowAttribute(const Name& attribute);
  bool RemoveAttribute(const Name& attribute);
  bool SetComments(bool allow) { if (comments == allow) return false; comments = allow; return true; }
  bool SetDataAttributes(bool allow);
  bool SetJavascriptURLs(bool allow) { if (javascriptURLs == allow) return false; javascriptURLs = allow; return true; }
  bool RemoveUnsafe(const std::vector<std::string>& eventAttributes);

 private:
  static std::string CanonicalAttributes(const Input::Element& element, ElementAttributes* out, bool report) {
    auto build = [&](const std::optional<std::vector<Name>>& source, std::optional<sanitizer::Set>* target,
                     const char* label) -> std::string {
      if (!source) return "";
      sanitizer::Set s;
      for (const Name& n : *source)
        if (!Insert(s, n) && report)
          return "Duplicate attribute " + n.Display() + " in '" + label + "' of " + element.name.Display() + ".";
      *target = std::move(s);
      return "";
    };
    std::string error = build(element.attributes, &out->attributes, "attributes");
    if (error.empty()) error = build(element.removeAttributes, &out->removeAttributes, "removeAttributes");
    if (!out->attributes && !out->removeAttributes) out->removeAttributes.emplace();
    return error;
  }
};
}  // namespace ruyijs::domain::document_model::sanitizer
#include "domain/document_model/sanitizer_operations.h"
#endif
