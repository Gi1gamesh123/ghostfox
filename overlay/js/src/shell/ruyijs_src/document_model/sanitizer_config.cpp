// Sanitizer (Firefox 158 Sanitizer.webidl, Exposed=Window): the JS face of the
// configuration model in domain/document_model/sanitizer_config.h. This file
// converts the WebIDL dictionaries and union arguments into that model and holds
// the Firefox default configuration; sanitizer_members.cpp holds the members.
#include "document_model/sanitizer.h"
#include "runtime/runtime.h"
#include "firefox_sanitizer_data.h"
#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include "js/PropertyAndElement.h"
#include <iterator>

namespace ruyijs::document_model {
namespace {
bool Text(JSContext* cx, JS::HandleValue value, std::string* out) {
  JS::RootedString text(cx, JS::ToString(cx, value));
  return text && ToUTF8(cx, text, out);
}
// sequence<...> conversion: an iterable object, else Firefox's sequence error.
bool Iterate(JSContext* cx, JS::HandleValue value, JS::ForOfIterator& it, const std::string& subject,
             std::string* error) {
  if (value.isObject() && it.init(value, JS::ForOfIterator::AllowNonIterable) && it.valueIsIterable()) return true;
  if (!JS_IsExceptionPending(cx)) *error = subject + " can't be converted to a sequence.";
  return false;
}
// A list of names; `element` selects the HTML default namespace.
bool Names(JSContext* cx, JS::HandleValue value, bool element, const std::string& where, const std::string& subject,
           std::vector<sz::Name>* out, std::string* error) {
  JS::ForOfIterator it(cx);
  if (!Iterate(cx, value, it, where + subject, error)) return error->size() > 0;
  JS::RootedValue item(cx);
  for (bool done = false;;) {
    if (!it.next(&item, &done)) return false;
    if (done) return true;
    sz::Name name;
    if (!SanitizerName(cx, item, element ? sz::kHtml : nullptr, element ? sz::kHtml : nullptr, where, &name, error))
      return false;
    if (!error->empty()) return true;
    out->push_back(std::move(name));
  }
}
}  // namespace

bool SanitizerName(JSContext* cx, JS::HandleValue value, const char* defaultNs, const char* dictNs,
                   const std::string& where, sz::Name* out, std::string* error) {
  if (!value.isObject()) {
    std::string name;
    if (!Text(cx, value, &name)) return false;
    *out = sz::Make(std::move(name), defaultNs);
    return true;
  }
  JS::RootedObject dict(cx, &value.toObject());
  JS::RootedValue field(cx);
  if (!JS_GetProperty(cx, dict, "name", &field)) return false;
  if (field.isUndefined()) {
    *error = where + "Missing required 'name' member of " +
             (defaultNs ? "SanitizerElementNamespace." : "SanitizerAttributeNamespace.");
    return true;
  }
  std::string name, ns = dictNs ? dictNs : "";
  bool nullNs = dictNs == nullptr;
  if (!Text(cx, field, &name) || !JS_GetProperty(cx, dict, "namespace", &field)) return false;
  if (!field.isUndefined()) {
    nullNs = field.isNull();
    if (!nullNs && !Text(cx, field, &ns)) return false;
  }
  if (ns.empty()) nullNs = true;  // canonicalize: "" means null
  *out = sz::Name{std::move(name), nullNs ? "" : ns, nullNs};
  return true;
}

bool SanitizerElement(JSContext* cx, JS::HandleValue value, const std::string& where, sz::Input::Element* out,
                      std::string* error) {
  if (!SanitizerName(cx, value, sz::kHtml, sz::kHtml, where, &out->name, error)) return false;
  if (!error->empty() || !value.isObject()) return true;
  JS::RootedObject dict(cx, &value.toObject());
  JS::RootedValue field(cx);
  const char* keys[] = {"attributes", "removeAttributes"};
  std::optional<std::vector<sz::Name>>* targets[] = {&out->attributes, &out->removeAttributes};
  for (int i = 0; i < 2; ++i) {
    if (!JS_GetProperty(cx, dict, keys[i], &field)) return false;
    if (field.isUndefined()) continue;
    targets[i]->emplace();
    const std::string subject = std::string("'") + keys[i] + "' member of SanitizerElementNamespaceWithAttributes";
    if (!Names(cx, field, false, where, subject, &**targets[i], error)) return false;
    if (!error->empty()) return true;
  }
  return true;
}

bool SanitizerInput(JSContext* cx, JS::HandleObject dict, sz::Input* in, std::string* error) {
  const std::string where = "Sanitizer constructor: ";
  JS::RootedValue field(cx);
  auto subject = [](const char* key) { return std::string("'") + key + "' member of SanitizerConfig"; };
  // Members in WebIDL dictionary order (lexicographic).
  if (!JS_GetProperty(cx, dict, "attributes", &field)) return false;
  if (!field.isUndefined()) {
    in->attributes.emplace();
    if (!Names(cx, field, false, where, subject("attributes"), &*in->attributes, error)) return false;
    if (!error->empty()) return true;
  }
  if (!JS_GetProperty(cx, dict, "comments", &field)) return false;
  if (!field.isUndefined()) in->comments = JS::ToBoolean(field);
  if (!JS_GetProperty(cx, dict, "dataAttributes", &field)) return false;
  if (!field.isUndefined()) in->dataAttributes = JS::ToBoolean(field);
  if (!JS_GetProperty(cx, dict, "elements", &field)) return false;
  if (!field.isUndefined()) {
    JS::ForOfIterator it(cx);
    if (!Iterate(cx, field, it, where + subject("elements"), error)) return !error->empty();
    in->elements.emplace();
    JS::RootedValue item(cx);
    for (bool done = false;;) {
      if (!it.next(&item, &done)) return false;
      if (done) break;
      sz::Input::Element element;
      if (!SanitizerElement(cx, item, where, &element, error)) return false;
      if (!error->empty()) return true;
      in->elements->push_back(std::move(element));
    }
  }
  if (!JS_GetProperty(cx, dict, "javascriptURLs", &field)) return false;
  if (!field.isUndefined()) in->javascriptURLs = JS::ToBoolean(field);
  const char* lists[] = {"removeAttributes", "removeElements", "replaceWithChildrenElements"};
  std::optional<std::vector<sz::Name>>* targets[] = {&in->removeAttributes, &in->removeElements,
                                                      &in->replaceWithChildrenElements};
  for (int i = 0; i < 3; ++i) {
    if (!JS_GetProperty(cx, dict, lists[i], &field)) return false;
    if (field.isUndefined()) continue;
    targets[i]->emplace();
    if (!Names(cx, field, i != 0, where, subject(lists[i]), &**targets[i], error)) return false;
    if (!error->empty()) return true;
  }
  return true;
}

void SanitizerDefault(sz::Config* config) {
  config->elements.emplace();
  for (const auto& element : kSanitizerDefaultElements) {
    sz::ElementAttributes a;
    a.attributes.emplace();
    for (const char* const* name = element.attributes; *name; ++name) a.attributes->push_back(sz::Make(*name, nullptr));
    config->elements->emplace_back(sz::Make(element.name, element.ns), std::move(a));
  }
  config->processingInstructions.emplace();
  config->attributes.emplace();
  for (const char* name : kSanitizerDefaultAttributes) config->attributes->push_back(sz::Make(name, nullptr));
  config->comments = false;
  config->dataAttributes = false;
  config->javascriptURLs = false;
}

std::vector<std::string> SanitizerEventAttributes() {
  return std::vector<std::string>(std::begin(kSanitizerEventAttributes), std::end(kSanitizerEventAttributes));
}
}  // namespace ruyijs::document_model
