#ifndef RUYIJS_RESOURCE_MULTIPART_H
#define RUYIJS_RESOURCE_MULTIPART_H
#include <string>
#include <vector>

namespace ruyijs::resource_network {
// One form-data part: the disposition name, the optional filename and part
// content type, and the raw bytes. No JS type lives here.
struct MultipartPart {
  std::string name;
  std::string filename;
  std::string content_type;
  std::string data;
};
// The boundary parameter of a multipart/form-data content type, or false when
// the type is not multipart or carries no boundary.
bool MultipartBoundary(const std::string& content_type, std::string* boundary);
// Splits a multipart body into its parts. False on a malformed body; the parser
// is bounded by the caller's body size and allocates only the parts it yields.
bool ParseMultipart(const std::string& body, const std::string& boundary,
                    std::vector<MultipartPart>* parts);
}  // namespace ruyijs::resource_network
#endif
