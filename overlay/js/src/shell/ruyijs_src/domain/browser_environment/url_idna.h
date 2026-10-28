#pragma once
// Host "domain to ASCII" for non-ASCII labels (WHATWG URL, UTS #46 ToASCII with
// Punycode, RFC 3492). The donor's ICU is built without IDNA, so this is the
// domain's own pure implementation: labels are lowercased with the simple case
// mappings of Latin-1, Latin Extended-A, Greek and Cyrillic, then Punycode-
// encoded with the "xn--" prefix. Full UTS #46 mapping and NFC normalization
// are not applied (docs/DEVIATIONS.md D143).
#include <cstdint>
#include <string>
#include <vector>

namespace ruyijs::domain::browser_environment::url_idna {
inline bool DecodeUtf8(const std::string& text, std::vector<uint32_t>* out) {
  for (size_t i = 0; i < text.size();) {
    const unsigned char lead = static_cast<unsigned char>(text[i]);
    const size_t extra = lead < 0x80 ? 0 : lead >= 0xF0 ? 3 : lead >= 0xE0 ? 2 : lead >= 0xC0 ? 1 : 4;
    if (extra == 4 || i + extra >= text.size()) return false;
    uint32_t code = extra == 0 ? lead : extra == 1 ? lead & 0x1F : extra == 2 ? lead & 0x0F : lead & 0x07;
    for (size_t k = 1; k <= extra; ++k) {
      const unsigned char next = static_cast<unsigned char>(text[i + k]);
      if ((next & 0xC0) != 0x80) return false;
      code = (code << 6) | (next & 0x3F);
    }
    out->push_back(code);
    i += extra + 1;
  }
  return true;
}
inline uint32_t SimpleLower(uint32_t c) {
  if (c >= 'A' && c <= 'Z') return c + 0x20;
  if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 0x20;
  if (c == 0x178) return 0xFF;
  if ((c >= 0x100 && c <= 0x137) || (c >= 0x14A && c <= 0x177)) return c | 1;
  if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E)) return (c & 1) ? c + 1 : c;
  if (c >= 0x391 && c <= 0x3A9 && c != 0x3A2) return c + 0x20;
  if (c == 0x386) return 0x3AC;
  if (c >= 0x388 && c <= 0x38A) return c + 0x25;
  if (c == 0x38C) return 0x3CC;
  if (c == 0x38E || c == 0x38F) return c + 0x3F;
  if (c >= 0x400 && c <= 0x40F) return c + 0x50;
  if (c >= 0x410 && c <= 0x42F) return c + 0x20;
  if ((c >= 0x460 && c <= 0x481) || (c >= 0x48A && c <= 0x4BF)) return c | 1;
  return c;
}
inline char Digit(uint32_t d) { return char(d < 26 ? 'a' + d : '0' + (d - 26)); }
inline uint32_t Adapt(uint32_t delta, uint32_t points, bool first) {
  delta = first ? delta / 700 : delta / 2;
  delta += delta / points;
  uint32_t k = 0;
  for (; delta > 455; k += 36) delta /= 35;
  return k + (36 * delta) / (delta + 38);
}
// RFC 3492 encoding of one label (base 36, tmin 1, tmax 26, skew 38, damp 700).
inline bool Punycode(const std::vector<uint32_t>& input, std::string* out) {
  uint32_t n = 128, delta = 0, bias = 72, handled = 0;
  for (uint32_t c : input) if (c < 0x80) { out->push_back(char(c)); ++handled; }
  const uint32_t basic = handled;
  if (basic) out->push_back('-');
  while (handled < input.size()) {
    uint32_t m = UINT32_MAX;
    for (uint32_t c : input) if (c >= n && c < m) m = c;
    if ((m - n) > (UINT32_MAX - delta) / (handled + 1)) return false;
    delta += (m - n) * (handled + 1);
    n = m;
    for (uint32_t c : input) {
      if (c < n && ++delta == 0) return false;
      if (c != n) continue;
      uint32_t q = delta;
      for (uint32_t k = 36;; k += 36) {
        const uint32_t t = k <= bias ? 1 : k >= bias + 26 ? 26 : k - bias;
        if (q < t) break;
        out->push_back(Digit(t + (q - t) % (36 - t)));
        q = (q - t) / (36 - t);
      }
      out->push_back(Digit(q));
      bias = Adapt(delta, handled + 1, handled == basic);
      delta = 0;
      ++handled;
    }
    ++delta;
    ++n;
  }
  return true;
}
// An all-ASCII host is returned unchanged (the caller lowercases and validates it).
inline bool ToAscii(const std::string& host, std::string* out) {
  bool ascii = true;
  for (unsigned char c : host) if (c >= 0x80) { ascii = false; break; }
  if (ascii) { *out = host; return true; }
  std::vector<uint32_t> points;
  if (!DecodeUtf8(host, &points)) return false;
  out->clear();
  std::vector<uint32_t> label;
  auto flush = [&]() {
    bool wide = false;
    for (uint32_t& c : label) { c = SimpleLower(c); wide = wide || c >= 0x80; }
    if (!wide) { for (uint32_t c : label) out->push_back(char(c)); return true; }
    out->append("xn--");
    return Punycode(label, out);
  };
  for (uint32_t c : points) {
    // UTS #46 label separators: '.', U+3002, U+FF0E, U+FF61.
    if (c == '.' || c == 0x3002 || c == 0xFF0E || c == 0xFF61) {
      if (!flush()) return false;
      out->push_back('.');
      label.clear();
    } else {
      label.push_back(c);
    }
  }
  return flush() && out->size() <= 253;
}
}  // namespace ruyijs::domain::browser_environment::url_idna
