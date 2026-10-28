// IntersectionObserver margin parsing and serialization. The donor delegates to
// its CSS parser with a restricted grammar (StyleParse for
// IntersectionObserverMargin): only Dimension tokens whose unit is "px" (ASCII
// case-insensitive) or Percentage tokens are accepted, one to four of them, and
// an exhausted input means 0px. Numbers are single-precision in the donor, so
// serialization writes the shortest decimal that round-trips the parsed float.
#include "web_observers/margin.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ruyijs::web_observers {
namespace {
bool Space(char16_t ch) {
  return ch == u' ' || ch == u'\t' || ch == u'\n' || ch == u'\f' || ch == u'\r';
}
bool Digit(char16_t ch) { return ch >= u'0' && ch <= u'9'; }
bool Letter(char16_t ch) {
  return (ch >= u'a' && ch <= u'z') || (ch >= u'A' && ch <= u'Z');
}
bool IdentChar(char16_t ch) {
  return Letter(ch) || Digit(ch) || ch == u'_' || ch == u'-';
}
// One number token followed immediately by either "%" or an identifier unit.
// The CSS tokenizer never splits between them, so whitespace here means the
// number stands alone and the donor's parser rejects it.
bool Component(const DomString& text, size_t* index, float* value, bool* percent) {
  size_t at = *index;
  const size_t begin = at;
  if (at < text.size() && (text[at] == u'+' || text[at] == u'-')) ++at;
  const size_t digits = at;
  while (at < text.size() && Digit(text[at])) ++at;
  if (at < text.size() && text[at] == u'.') {
    ++at;
    while (at < text.size() && Digit(text[at])) ++at;
  }
  if (at == digits || (at == digits + 1 && text[digits] == u'.')) return false;
  if (at < text.size() && (text[at] == u'e' || text[at] == u'E')) {
    size_t exponent = at + 1;
    if (exponent < text.size() && (text[exponent] == u'+' || text[exponent] == u'-'))
      ++exponent;
    const size_t exponent_digits = exponent;
    while (exponent < text.size() && Digit(text[exponent])) ++exponent;
    if (exponent != exponent_digits) at = exponent;
  }
  char number[64];
  const size_t length = at - begin;
  if (length == 0 || length >= sizeof(number)) return false;
  for (size_t unit = 0; unit < length; ++unit)
    number[unit] = static_cast<char>(text[begin + unit]);
  number[length] = 0;
  *value = std::strtof(number, nullptr);
  if (at < text.size() && text[at] == u'%') {
    *percent = true;
    *index = at + 1;
    return true;
  }
  const size_t unit_begin = at;
  while (at < text.size() && IdentChar(text[at])) ++at;
  const size_t unit_length = at - unit_begin;
  *percent = false;
  if (unit_length != 2) return false;
  const char16_t first = text[unit_begin];
  const char16_t second = text[unit_begin + 1];
  if ((first != u'p' && first != u'P') || (second != u'x' && second != u'X')) return false;
  *index = at;
  return true;
}
std::string Number(float value) {
  char buffer[64];
  if (value == static_cast<double>(static_cast<long long>(value)) &&
      std::fabs(value) < 1e15) {
    std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(value));
    return buffer;
  }
  for (int precision = 1; precision <= 17; ++precision) {
    std::snprintf(buffer, sizeof(buffer), "%.*g", precision, double(value));
    if (std::strtof(buffer, nullptr) != value) continue;
    if (std::strchr(buffer, 'e') || std::strchr(buffer, 'E')) continue;
    return buffer;
  }
  std::snprintf(buffer, sizeof(buffer), "%.17g", double(value));
  return buffer;
}
}  // namespace

bool ParseMargin(const DomString& text, Margin* margin) {
  float value[4];
  bool percent[4];
  size_t count = 0;
  size_t index = 0;
  while (index < text.size() && Space(text[index])) ++index;
  while (index < text.size()) {
    if (count == 4) return false;
    if (!Component(text, &index, &value[count], &percent[count])) return false;
    ++count;
    while (index < text.size() && Space(text[index])) ++index;
  }
  if (count == 0) {
    *margin = Margin();
    return true;
  }
  if (count == 1) value[3] = value[2] = value[1] = value[0];
  if (count == 1) percent[3] = percent[2] = percent[1] = percent[0];
  if (count == 2) { value[2] = value[0]; value[3] = value[1]; }
  if (count == 2) { percent[2] = percent[0]; percent[3] = percent[1]; }
  if (count == 3) { value[3] = value[1]; percent[3] = percent[1]; }
  margin->top = value[0];
  margin->right = value[1];
  margin->bottom = value[2];
  margin->left = value[3];
  margin->percent_top = percent[0];
  margin->percent_right = percent[1];
  margin->percent_bottom = percent[2];
  margin->percent_left = percent[3];
  return true;
}

DomString SerializeMargin(const Margin& margin) {
  const struct { double value; bool percent; } sides[] = {
      {margin.top, margin.percent_top}, {margin.right, margin.percent_right},
      {margin.bottom, margin.percent_bottom}, {margin.left, margin.percent_left}};
  std::string out;
  for (size_t index = 0; index < 4; ++index) {
    if (index) out.push_back(' ');
    out += Number(static_cast<float>(sides[index].value));
    out += sides[index].percent ? "%" : "px";
  }
  DomString result;
  result.reserve(out.size());
  for (char ch : out) result.push_back(char16_t(static_cast<unsigned char>(ch)));
  return result;
}
}  // namespace ruyijs::web_observers
