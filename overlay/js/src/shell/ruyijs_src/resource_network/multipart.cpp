// The multipart/form-data parser behind Body.formData(): a bounded splitter over
// the response bytes, not a second body store. It reads the boundary from the
// content type, walks each part's headers and yields the disposition name,
// filename, part content type and bytes. No V8/JSAPI type is used here.
#include "resource_network/multipart.h"

#include <cstddef>

namespace ruyijs::resource_network {
namespace {
std::string Trim(const std::string& text) {
  size_t begin = 0;
  size_t end = text.size();
  while (begin < end && (text[begin] == ' ' || text[begin] == '\t')) ++begin;
  while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t')) --end;
  return text.substr(begin, end - begin);
}

std::string Lower(std::string text) {
  for (char& ch : text)
    if (ch >= 'A' && ch <= 'Z') ch = char(ch - 'A' + 'a');
  return text;
}

// The value of a `; key=value` parameter inside a header value; a quoted value
// keeps no quotes. Key matching is ASCII case-insensitive.
bool Parameter(const std::string& header, const char* key, std::string* out) {
  size_t pos = 0;
  while (pos <= header.size()) {
    const size_t semi = header.find(';', pos);
    const std::string segment =
        header.substr(pos, (semi == std::string::npos ? header.size() : semi) - pos);
    const size_t equals = segment.find('=');
    if (equals != std::string::npos && Lower(Trim(segment.substr(0, equals))) == key) {
      std::string value = Trim(segment.substr(equals + 1));
      if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        value = value.substr(1, value.size() - 2);
      *out = value;
      return true;
    }
    if (semi == std::string::npos) break;
    pos = semi + 1;
  }
  return false;
}

void ParseHeaders(const std::string& block, MultipartPart* part) {
  size_t pos = 0;
  while (pos < block.size()) {
    const size_t eol = block.find("\r\n", pos);
    const std::string line =
        block.substr(pos, (eol == std::string::npos ? block.size() : eol) - pos);
    const size_t colon = line.find(':');
    if (colon != std::string::npos) {
      const std::string name = Lower(Trim(line.substr(0, colon)));
      const std::string value = Trim(line.substr(colon + 1));
      if (name == "content-disposition") {
        Parameter(value, "name", &part->name);
        Parameter(value, "filename", &part->filename);
      } else if (name == "content-type") {
        part->content_type = value;
      }
    }
    if (eol == std::string::npos) break;
    pos = eol + 2;
  }
}
}  // namespace

bool MultipartBoundary(const std::string& content_type, std::string* boundary) {
  const size_t semi = content_type.find(';');
  const std::string essence =
      Lower(Trim(content_type.substr(0, semi == std::string::npos ? content_type.size() : semi)));
  if (essence != "multipart/form-data") return false;
  return Parameter(content_type, "boundary", boundary);
}

bool ParseMultipart(const std::string& body, const std::string& boundary,
                    std::vector<MultipartPart>* parts) {
  if (boundary.empty()) return false;
  const std::string delimiter = "--" + boundary;
  size_t pos = body.find(delimiter);
  if (pos == std::string::npos) return false;
  pos += delimiter.size();
  while (true) {
    if (pos > body.size()) return false;
    if (body.compare(pos, 2, "--") == 0) return true;  // closing delimiter
    if (body.compare(pos, 2, "\r\n") != 0) return false;
    pos += 2;
    const size_t header_end = body.find("\r\n\r\n", pos);
    if (header_end == std::string::npos) return false;
    const size_t content_start = header_end + 4;
    const size_t next = body.find(delimiter, content_start);
    if (next == std::string::npos || next < content_start + 2) return false;
    const size_t content_end = next - 2;
    if (body.compare(content_end, 2, "\r\n") != 0) return false;
    MultipartPart part;
    ParseHeaders(body.substr(pos, header_end - pos), &part);
    part.data = body.substr(content_start, content_end - content_start);
    parts->push_back(part);
    pos = next + delimiter.size();
  }
}
}  // namespace ruyijs::resource_network
