#pragma once
#include "domain/document_model/element.h"
#include <cmath>
#include <cstdlib>
#include <limits>

namespace ruyijs::domain::document_model {
inline DomString input_type(DomStringView raw) {
  const auto value = ascii_lower(raw);
  // Firefox157's production binding has no month/week input implementation.
  for (auto name : {u"text",u"search",u"tel",u"url",u"email",u"password",u"date",
      u"time",u"datetime-local",u"number",u"range",u"color",u"checkbox",u"radio",
      u"file",u"submit",u"image",u"reset",u"button",u"hidden"})
    if (value == name) return value;
  return u"text";
}
inline DomString input_type(const Element& e) {
  return input_type(e.get_attribute_ns16({}, u"type").value_or(u""));
}
inline bool input_numeric(DomStringView type) {
  return type == u"number" || type == u"range" || type == u"date" || type == u"month" ||
      type == u"week" || type == u"time" || type == u"datetime-local";
}
inline bool input_textual(DomStringView type) {
  return type == u"text" || type == u"search" || type == u"tel" || type == u"url" ||
      type == u"email" || type == u"password";
}
inline bool input_default_mode(DomStringView type) {
  return type == u"hidden" || type == u"submit" || type == u"image" ||
      type == u"reset" || type == u"button" || type == u"checkbox" || type == u"radio";
}
inline DomString input_sanitize(DomStringView type, DomStringView raw) {
  DomString value(raw);
  if (input_textual(type)) {
    value.erase(std::remove_if(value.begin(), value.end(), [](char16_t c) {
      return c == u'\r' || c == u'\n';
    }), value.end());
    if (type == u"url" || type == u"email") {
      const auto first = value.find_first_not_of(u" \t\r\n\f");
      value = first == DomString::npos ? DomString{} :
          value.substr(first, value.find_last_not_of(u" \t\r\n\f") - first + 1);
    }
  }
  return value;
}
inline DomString input_value(const Element& e) {
  const auto type = input_type(e);
  if (type == u"file") return u"";
  if (!input_default_mode(type) && e.input_current_value()) return *e.input_current_value();
  const auto value = e.get_attribute_ns16({}, u"value").value_or(
      type == u"checkbox" || type == u"radio" ? u"on" : u"");
  return input_default_mode(type) ? value : input_sanitize(type, value);
}
inline double input_decimal(DomStringView value) {
  const auto raw = to_utf8(value); char* end = nullptr;
  const double number = std::strtod(raw.c_str(), &end);
  return end == raw.c_str() ? std::numeric_limits<double>::quiet_NaN() : number;
}
inline double input_integer(DomStringView name, DomStringView value) {
  const double fallback = name == u"size" ? 20 : name == u"maxlength" || name == u"minlength" ? -1 : 0;
  const auto raw = to_utf8(value); char* end = nullptr;
  const auto parsed = std::strtoll(raw.c_str(), &end, 10);
  if (end == raw.c_str()) return fallback;
  const uint32_t number = static_cast<uint32_t>(parsed);
  if (name == u"size") return number || !parsed ? double(number) : 20;
  if (name == u"maxlength" || name == u"minlength")
    return parsed < 0 || parsed > INT32_MAX ? -1 : double(parsed);
  return double(number);
}
inline DomString input_text(const Element& e, DomStringView name) {
  if (name == u"type") return input_type(e);
  if (name == u"value") return input_value(e);
  if (name == u"defaultValue") name = u"value";
  return e.get_attribute16(name).value_or(u"");
}
inline bool input_will_validate(const Element& e) {
  const auto type = input_type(e);
  return !e.get_attribute16(u"disabled") && !e.get_attribute16(u"readonly") &&
      type != u"hidden" && type != u"reset" && type != u"button";
}
}
