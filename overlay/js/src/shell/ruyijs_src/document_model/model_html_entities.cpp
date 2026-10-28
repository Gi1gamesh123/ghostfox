#include "document_model/model_html.h"
#include <cstdint>
#include <string>

namespace ruyijs::document_model::html {
namespace {
struct Named { const char16_t* name; char16_t value; bool legacy; };  // legacy: no ';' needed
// Subset of the WHATWG named reference table (docs/DEVIATIONS.md D7); unknown
// names stay literal text.
const Named kNamed[] = {
  {u"amp", u'&', true}, {u"AMP", u'&', true}, {u"lt", u'<', true}, {u"LT", u'<', true},
  {u"gt", u'>', true}, {u"GT", u'>', true}, {u"quot", u'"', true}, {u"QUOT", u'"', true},
  {u"apos", u'\'', false}, {u"nbsp", 0xA0, true}, {u"iexcl", 0xA1, true}, {u"cent", 0xA2, true},
  {u"pound", 0xA3, true}, {u"curren", 0xA4, true}, {u"yen", 0xA5, true}, {u"brvbar", 0xA6, true},
  {u"sect", 0xA7, true}, {u"uml", 0xA8, true}, {u"copy", 0xA9, true}, {u"COPY", 0xA9, true},
  {u"ordf", 0xAA, true}, {u"laquo", 0xAB, true}, {u"not", 0xAC, true}, {u"shy", 0xAD, true},
  {u"reg", 0xAE, true}, {u"REG", 0xAE, true}, {u"macr", 0xAF, true}, {u"deg", 0xB0, true},
  {u"plusmn", 0xB1, true}, {u"sup2", 0xB2, true}, {u"sup3", 0xB3, true}, {u"acute", 0xB4, true},
  {u"micro", 0xB5, true}, {u"para", 0xB6, true}, {u"middot", 0xB7, true}, {u"cedil", 0xB8, true},
  {u"sup1", 0xB9, true}, {u"ordm", 0xBA, true}, {u"raquo", 0xBB, true}, {u"frac14", 0xBC, true},
  {u"frac12", 0xBD, true}, {u"frac34", 0xBE, true}, {u"iquest", 0xBF, true}, {u"times", 0xD7, true},
  {u"divide", 0xF7, true}, {u"euro", 0x20AC, false}, {u"trade", 0x2122, false},
  {u"hellip", 0x2026, false}, {u"mdash", 0x2014, false}, {u"ndash", 0x2013, false},
  {u"lsquo", 0x2018, false}, {u"rsquo", 0x2019, false}, {u"sbquo", 0x201A, false},
  {u"ldquo", 0x201C, false}, {u"rdquo", 0x201D, false}, {u"bdquo", 0x201E, false},
  {u"bull", 0x2022, false}, {u"dagger", 0x2020, false}, {u"Dagger", 0x2021, false},
  {u"permil", 0x2030, false}, {u"prime", 0x2032, false}, {u"lsaquo", 0x2039, false},
  {u"rsaquo", 0x203A, false}, {u"larr", 0x2190, false}, {u"uarr", 0x2191, false},
  {u"rarr", 0x2192, false}, {u"darr", 0x2193, false}, {u"harr", 0x2194, false},
  {u"ensp", 0x2002, false}, {u"emsp", 0x2003, false}, {u"thinsp", 0x2009, false},
  {u"zwnj", 0x200C, false}, {u"zwj", 0x200D, false}, {u"lrm", 0x200E, false},
  {u"rlm", 0x200F, false}, {u"infin", 0x221E, false}, {u"ne", 0x2260, false},
  {u"le", 0x2264, false}, {u"ge", 0x2265, false}, {u"hearts", 0x2665, false},
};
// Numeric references in 0x80-0x9F name windows-1252 characters.
const char16_t kWindows1252[32] = {
  0x20AC, 0x81, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
  0x2039, 0x0152, 0x8D, 0x017D, 0x8F, 0x90, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
  0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x9D, 0x017E, 0x0178};
int Digit(char16_t c, bool hex) {
  if (c >= u'0' && c <= u'9') return c - u'0';
  if (hex && c >= u'a' && c <= u'f') return c - u'a' + 10;
  if (hex && c >= u'A' && c <= u'F') return c - u'A' + 10;
  return -1;
}
bool Alnum(char16_t c) { return Digit(c, true) >= 0 || (c >= u'g' && c <= u'z') || (c >= u'G' && c <= u'Z'); }
size_t Numeric(DomStringView input, size_t pos, DomString& out) {
  size_t at = pos + 2;
  bool hex = at < input.size() && (input[at] == u'x' || input[at] == u'X');
  if (hex) ++at;
  const size_t start = at;
  uint32_t code = 0;
  for (int digit; at < input.size() && (digit = Digit(input[at], hex)) >= 0; ++at)
    if (code <= 0x10FFFF) code = code * (hex ? 16 : 10) + uint32_t(digit);
  if (at == start) return 0;  // "&#" or "&#x" without digits stays literal.
  if (at < input.size() && input[at] == u';') ++at;
  if (code == 0 || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) code = 0xFFFD;
  else if (code >= 0x80 && code <= 0x9F) code = kWindows1252[code - 0x80];
  if (code > 0xFFFF) {
    code -= 0x10000;
    out += char16_t(0xD800 + (code >> 10));
    out += char16_t(0xDC00 + (code & 0x3FF));
  } else {
    out += char16_t(code);
  }
  return at - pos;
}
}
size_t DecodeReference(DomStringView input, size_t pos, bool attribute, DomString& out) {
  const size_t at = pos + 1;
  if (at < input.size() && input[at] == u'#') return Numeric(input, pos, out);
  const Named* best = nullptr;
  size_t best_length = 0;
  bool semicolon = false;
  for (const auto& entry : kNamed) {  // Longest matching name wins.
    const size_t length = std::char_traits<char16_t>::length(entry.name);
    if (length <= best_length || input.substr(at, length) != entry.name) continue;
    const bool closed = at + length < input.size() && input[at + length] == u';';
    if (!closed && !entry.legacy) continue;
    best = &entry;
    best_length = length;
    semicolon = closed;
  }
  if (!best) return 0;
  const size_t next = at + best_length;
  // Historical attribute rule: "&amp=" or "&ampx" without ';' stays literal.
  if (!semicolon && attribute && next < input.size() && (input[next] == u'=' || Alnum(input[next])))
    return 0;
  out += best->value;
  return 1 + best_length + (semicolon ? 1 : 0);
}
}
