// Validity and the modifying operations of the HTML Sanitizer configuration
// (Firefox 158 Sanitizer.cpp IsValid, AllowElementInternal, RemoveElementCanonical,
// ReplaceElementWithChildren, AllowAttribute, RemoveAttributeCanonical,
// SetDataAttributes, RemoveUnsafe), step for step. Included by sanitizer_config.h.
#ifndef RUYIJS_DOMAIN_DOCUMENT_MODEL_SANITIZER_OPERATIONS_H
#define RUYIJS_DOMAIN_DOCUMENT_MODEL_SANITIZER_OPERATIONS_H

namespace ruyijs::domain::document_model::sanitizer {
inline bool NonReplaceable(const Name& n) {
  return n == Make("html", kHtml) || n == Make("svg", kSvg) || n == Make("math", kMathml);
}
inline std::string Config::Valid() const {
  if (elements && removeElements) return "'elements' and 'removeElements' are not allowed at the same time.";
  if (processingInstructions && removeProcessingInstructions)
    return "'processingInstructions' and 'removeProcessingInstructions' are not allowed at the same time.";
  if (attributes && removeAttributes) return "'attributes' and 'removeAttributes' are not allowed at the same time.";
  if (replaceWithChildrenElements) {
    for (const Name& e : *replaceWithChildrenElements)
      if (NonReplaceable(e)) return "Element " + e.Display() + " is not allowed in 'replaceWithChildrenElements'";
    if (elements) {
      for (const auto& e : *elements)
        if (Has(*replaceWithChildrenElements, e.first))
          return "Element " + e.first.Display() + " can't be in both 'elements' and 'replaceWithChildrenElements'.";
    } else {
      for (const Name& e : *removeElements)
        if (Has(*replaceWithChildrenElements, e))
          return "Element " + e.Display() + " can't be in both 'removeElements' and 'replaceWithChildrenElements'.";
    }
  }
  if (attributes) {
    if (elements) {
      for (const auto& [name, a] : *elements) {
        if (a.attributes)
          for (const Name& n : *a.attributes)
            if (Has(*attributes, n))
              return "Attribute " + n.Display() + " can't be part of both the 'attributes' of the element " +
                     name.Display() + " and the global 'attributes'.";
        if (a.removeAttributes)
          for (const Name& n : *a.removeAttributes)
            if (!Has(*attributes, n))
              return "Attribute " + n.Display() + " can't be in 'removeAttributes' of the element " + name.Display() +
                     " but not in the global 'attributes'.";
        if (*dataAttributes && a.attributes)
          for (const Name& n : *a.attributes)
            if (n.IsData())
              return "Data attribute " + n.Display() + " in the 'attributes' of the element " + name.Display() +
                     " is redundant with 'dataAttributes' being true.";
      }
    }
    if (*dataAttributes)
      for (const Name& n : *attributes)
        if (n.IsData())
          return "Data attribute " + n.Display() + " in the global 'attributes' is redundant with 'dataAttributes' being true.";
  }
  if (removeAttributes) {
    if (elements) {
      for (const auto& [name, a] : *elements) {
        if (a.attributes && a.removeAttributes)
          return "Element " + name.Display() + " can't have both 'attributes' and 'removeAttributes'.";
        if (a.attributes)
          for (const Name& n : *a.attributes)
            if (Has(*removeAttributes, n))
              return "Attribute " + n.Display() + " can't be in 'attributes' of the element " + name.Display() +
                     " while in the global 'removeAttributes'.";
        if (a.removeAttributes)
          for (const Name& n : *a.removeAttributes)
            if (Has(*removeAttributes, n))
              return "Attribute " + n.Display() + " can't be part of both the 'removeAttributes' of the element " +
                     name.Display() + " and the global 'removeAttributes'.";
      }
    }
    if (dataAttributes) return "'removeAttributes' and 'dataAttributes' aren't allowed at the same time.";
  }
  return "";
}

inline bool Config::AllowElement(const Input::Element& input) {
  const Name element = input.name;
  ElementAttributes a;
  CanonicalAttributes(input, &a, false);  // duplicates are ignored here
  if (elements) {
    bool modified = replaceWithChildrenElements ? Erase(*replaceWithChildrenElements, element) : false;
    if (attributes) {
      if (a.attributes) {
        sanitizer::Set kept;
        for (const Name& n : *a.attributes)
          if (!Has(*attributes, n) && !(*dataAttributes && n.IsData())) kept.push_back(n);
        a.attributes = std::move(kept);
      }
      if (a.removeAttributes) {
        sanitizer::Set kept;
        for (const Name& n : *a.removeAttributes) if (Has(*attributes, n)) kept.push_back(n);
        a.removeAttributes = std::move(kept);
      }
    } else {
      if (a.attributes) {
        sanitizer::Set kept;
        for (const Name& n : *a.attributes)
          if (!(a.removeAttributes && Has(*a.removeAttributes, n)) && !Has(*removeAttributes, n)) kept.push_back(n);
        a.attributes = std::move(kept);
        a.removeAttributes.reset();
      }
      if (a.removeAttributes) {
        sanitizer::Set kept;
        for (const Name& n : *a.removeAttributes) if (!Has(*removeAttributes, n)) kept.push_back(n);
        a.removeAttributes = std::move(kept);
      }
    }
    ElementAttributes* existing = Find(*elements, element);
    if (!existing) { elements->emplace_back(element, std::move(a)); return true; }
    if (a == *existing) return modified;
    *existing = std::move(a);
    return true;
  }
  if (a.attributes || (a.removeAttributes && !a.removeAttributes->empty())) return false;
  bool modified = replaceWithChildrenElements ? Erase(*replaceWithChildrenElements, element) : false;
  if (!Has(*removeElements, element)) return modified;
  Erase(*removeElements, element);
  return true;
}
inline bool Config::RemoveElement(const Name& element) {
  bool modified = replaceWithChildrenElements ? Erase(*replaceWithChildrenElements, element) : false;
  if (elements) {
    for (auto it = elements->begin(); it != elements->end(); ++it)
      if (it->first == element) { elements->erase(it); return true; }
    return modified;
  }
  if (Has(*removeElements, element)) return modified;
  removeElements->push_back(element);
  return true;
}
inline bool Config::ReplaceElementWithChildren(const Name& element) {
  if (NonReplaceable(element)) return false;
  if (replaceWithChildrenElements && Has(*replaceWithChildrenElements, element)) return false;
  if (removeElements) {
    Erase(*removeElements, element);
  } else {
    for (auto it = elements->begin(); it != elements->end(); ++it)
      if (it->first == element) { elements->erase(it); break; }
  }
  if (!replaceWithChildrenElements) replaceWithChildrenElements.emplace();
  replaceWithChildrenElements->push_back(element);
  return true;
}
inline bool Config::AllowAttribute(const Name& attribute) {
  if (attributes) {
    if (*dataAttributes && attribute.IsData()) return false;
    if (Has(*attributes, attribute)) return false;
    if (elements)
      for (auto& e : *elements)
        if (e.second.attributes) Erase(*e.second.attributes, attribute);
    attributes->push_back(attribute);
    return true;
  }
  return Erase(*removeAttributes, attribute);
}
inline bool Config::RemoveAttribute(const Name& attribute) {
  if (attributes) {
    bool modified = Erase(*attributes, attribute);
    if (elements)
      for (auto& e : *elements) {
        if (e.second.attributes && Erase(*e.second.attributes, attribute)) modified = true;
        if (e.second.removeAttributes) Erase(*e.second.removeAttributes, attribute);
      }
    return modified;
  }
  if (Has(*removeAttributes, attribute)) return false;
  if (elements)
    for (auto& e : *elements) {
      if (e.second.attributes) Erase(*e.second.attributes, attribute);
      if (e.second.removeAttributes) Erase(*e.second.removeAttributes, attribute);
    }
  removeAttributes->push_back(attribute);
  return true;
}
inline bool Config::SetDataAttributes(bool allow) {
  if (!attributes) return false;
  if (*dataAttributes == allow) return false;
  if (allow) {
    auto drop = [](sanitizer::Set& s) { s.erase(std::remove_if(s.begin(), s.end(), [](const Name& n) { return n.IsData(); }), s.end()); };
    drop(*attributes);
    if (elements) for (auto& e : *elements) if (e.second.attributes) drop(*e.second.attributes);
  }
  dataAttributes = allow;
  return true;
}
inline bool Config::RemoveUnsafe(const std::vector<std::string>& eventAttributes) {
  bool result = false;
  const Name unsafe[] = {Make("base", kHtml), Make("embed", kHtml), Make("frame", kHtml), Make("iframe", kHtml),
                         Make("object", kHtml), Make("script", kHtml), Make("script", kSvg), Make("use", kSvg)};
  for (const Name& e : unsafe) if (RemoveElement(e)) result = true;
  for (const std::string& name : eventAttributes) if (RemoveAttribute(Make(name, nullptr))) result = true;
  if (javascriptURLs) { result = true; javascriptURLs = false; }
  return result;
}
}  // namespace ruyijs::domain::document_model::sanitizer
#endif
