// Original Element's typed viewBox value. No JS or Geometry owner dependency.
#pragma once
#include "domain/document_model/dom_string.h"
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <system_error>

namespace ruyijs::domain::document_model {
struct SvgViewBoxState {
  float fields[4] = {0, 0, 0, 0};
  bool hasBaseValue = false, none = true;
};
inline bool SvgViewBoxSpace(char16_t c) {
  return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r' || c == u'\f';
}
inline bool SvgViewBoxDigit(char16_t c) { return c >= u'0' && c <= u'9'; }
inline bool SvgViewBoxNumber(DomStringView text, float* result) {
  if (text.empty()) return false;
  size_t p = 0;
  const bool negative = text[p] == u'-';
  if (text[p] == u'+' || negative) ++p;
  const size_t digits = p;
  while (p < text.size() && SvgViewBoxDigit(text[p])) ++p;
  const size_t integers = p - digits;
  if (p < text.size() && text[p] == u'.') {
    const size_t fraction = ++p;
    while (p < text.size() && SvgViewBoxDigit(text[p])) ++p;
    if (p == fraction) return false;
  } else if (p == digits) return false;
  size_t exponentAt = p;
  int exponent = 0;
  if (p < text.size() && (text[p] == u'e' || text[p] == u'E')) {
    ++p; bool minus = p < text.size() && text[p] == u'-';
    if (p < text.size() && (text[p] == u'+' || minus)) ++p;
    const size_t first = p;
    while (p < text.size() && SvgViewBoxDigit(text[p])) {
      if (exponent < 1000000) exponent = exponent * 10 + int(text[p] - u'0');
      ++p;
    }
    if (p == first) return false;
    if (minus) exponent = -exponent;
  }
  if (p != text.size()) return false;
  std::string ascii;
  ascii.reserve(text.size());
  for (char16_t c : text) ascii.push_back(char(c));
  const char* begin = ascii.data() + (ascii[0] == '+' ? 1 : 0);
  auto parsed = std::from_chars(begin, ascii.data() + ascii.size(), *result, std::chars_format::general);
  if (parsed.ec == std::errc{} && parsed.ptr == ascii.data() + ascii.size()) return std::isfinite(*result);
  if (parsed.ec != std::errc::result_out_of_range) return false;
  // from_chars reports underflow as out-of-range; SVG accepts signed zero.
  // Classify by the effective decimal exponent, including leading zero digits.
  size_t ordinal = 0, firstNonzero = 0; bool found = false;
  for (size_t i = digits; i < exponentAt; ++i) if (text[i] != u'.') {
    if (!found && text[i] != u'0') { firstNonzero = ordinal; found = true; }
    ++ordinal;
  }
  const double order = double(integers) - double(firstNonzero) - 1 + exponent;
  if (found && order >= -38) return false;
  *result = negative ? -0.0f : 0.0f;
  return true;
}
inline SvgViewBoxState ParseSvgViewBox(DomStringView text) {
  SvgViewBoxState result;
  if (text == u"none") { result.hasBaseValue = true; return result; }
  size_t p = 0;
  for (unsigned i = 0; i < 4; ++i) {
    while (p < text.size() && SvgViewBoxSpace(text[p])) ++p;
    const size_t start = p;
    while (p < text.size() && !SvgViewBoxSpace(text[p]) && text[p] != u',') ++p;
    if (!SvgViewBoxNumber(text.substr(start, p - start), &result.fields[i])) return {};
    while (p < text.size() && SvgViewBoxSpace(text[p])) ++p;
    if (i < 3 && p < text.size() && text[p] == u',') ++p;
  }
  if (p != text.size() || result.fields[2] < 0 || result.fields[3] < 0) return {};
  result.hasBaseValue = true; result.none = false;
  return result;
}
inline DomString SerializeSvgViewBox(const SvgViewBoxState& value) {
  DomString result;
  for (unsigned i = 0; i < 4; ++i) {
    char buffer[64];
    const auto text = std::to_chars(buffer, buffer + sizeof buffer, value.fields[i], std::chars_format::general, 6);
    if (text.ec != std::errc{}) throw std::length_error("SVG viewBox serialization failed");
    if (i) result += u' ';
    for (const char* p = buffer; p != text.ptr; ++p) result += char16_t(*p);
  }
  return result;
}
}
