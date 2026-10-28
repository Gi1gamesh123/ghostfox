#pragma once
// WHATWG Fetch "data: URL processor": the MIME type and the body bytes a data:
// URL denotes. Pure: no network, no engine. The caller passes the serialized URL
// (the fragment is ignored here, as the processor excludes it).
#include <cstdint>
#include <string>

namespace ruyijs::domain::resource_network {

struct DataUrl {
  std::string mimeType;  // serialized, e.g. "text/plain;charset=US-ASCII"
  std::string body;      // bytes
};

namespace data_url_detail {
inline bool AsciiWhitespace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\f' || c == '\r';
}
inline std::string Trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && AsciiWhitespace(s[a])) ++a;
  while (b > a && AsciiWhitespace(s[b - 1])) --b;
  return s.substr(a, b - a);
}
inline std::string Lower(std::string s) {
  for (char& c : s) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  return s;
}
inline int Hex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
// URL percent-decode: "%XY" with two hex digits becomes one byte; anything else stays.
inline std::string PercentDecode(const std::string& in) {
  std::string out;
  out.reserve(in.size());
  for (size_t i = 0; i < in.size(); ++i) {
    if (in[i] == '%' && i + 2 < in.size() && Hex(in[i + 1]) >= 0 && Hex(in[i + 2]) >= 0) {
      out.push_back(char(Hex(in[i + 1]) * 16 + Hex(in[i + 2])));
      i += 2;
    } else {
      out.push_back(in[i]);
    }
  }
  return out;
}
// Infra "forgiving-base64 decode"; false is failure.
inline bool ForgivingBase64(const std::string& in, std::string* out) {
  std::string data;
  for (char c : in) if (!AsciiWhitespace(c)) data.push_back(c);
  if (data.size() % 4 == 0 && !data.empty() && data.back() == '=') {
    data.pop_back();
    if (!data.empty() && data.back() == '=') data.pop_back();
  }
  if (data.size() % 4 == 1) return false;
  auto value = [](char c) -> int {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
  };
  out->clear();
  uint32_t buffer = 0;
  int bits = 0;
  for (char c : data) {
    const int v = value(c);
    if (v < 0) return false;
    buffer = (buffer << 6) | uint32_t(v);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out->push_back(char((buffer >> bits) & 0xff));
    }
  }
  return true;
}
// A minimal MIME type parse-and-serialize: "type/subtype" lower-cased followed by
// its parameters as given (trimmed); empty when the essence is not type/subtype.
inline std::string SerializeMime(const std::string& input) {
  const size_t semi = input.find(';');
  const std::string essence = Lower(Trim(input.substr(0, semi)));
  const size_t slash = essence.find('/');
  if (slash == std::string::npos || slash == 0 || slash + 1 == essence.size() ||
      essence.find_first_of(" \t\r\n\f\"(),:<=>?@[\\]{}", 0) != std::string::npos)
    return std::string();
  std::string out = essence;
  for (size_t at = semi; at != std::string::npos;) {
    const size_t next = input.find(';', at + 1);
    const std::string param = Trim(input.substr(at + 1, next == std::string::npos ? std::string::npos
                                                                                  : next - at - 1));
    const size_t eq = param.find('=');
    if (eq != std::string::npos && eq > 0 && eq + 1 < param.size())
      out += ";" + Lower(param.substr(0, eq)) + "=" + param.substr(eq + 1);
    at = next;
  }
  return out;
}
}  // namespace data_url_detail

// False when the URL is not a data: URL or has no comma (a network error in Fetch).
inline bool ProcessDataUrl(const std::string& url, DataUrl* out) {
  using namespace data_url_detail;
  if (url.size() < 5 || Lower(url.substr(0, 5)) != "data:") return false;
  std::string input = url.substr(5, url.find('#') == std::string::npos ? std::string::npos
                                                                       : url.find('#') - 5);
  const size_t comma = input.find(',');
  if (comma == std::string::npos) return false;
  std::string mime = Trim(input.substr(0, comma));
  std::string body = PercentDecode(input.substr(comma + 1));
  // ";base64" at the end (case-insensitive, spaces allowed before it after the ';').
  const std::string lowered = Lower(mime);
  if (lowered.size() >= 6 && lowered.compare(lowered.size() - 6, 6, "base64") == 0) {
    size_t at = lowered.size() - 6;
    while (at > 0 && lowered[at - 1] == ' ') --at;
    if (at > 0 && lowered[at - 1] == ';') {
      std::string decoded;
      if (!ForgivingBase64(body, &decoded)) return false;
      body = std::move(decoded);
      mime = mime.substr(0, at - 1);
    }
  }
  if (!mime.empty() && mime[0] == ';') mime = "text/plain" + mime;
  std::string serialized = SerializeMime(mime);
  if (serialized.empty()) serialized = "text/plain;charset=US-ASCII";
  out->mimeType = std::move(serialized);
  out->body = std::move(body);
  return true;
}

}  // namespace ruyijs::domain::resource_network
