#ifndef RUYIJS_MODEL_HEADERS_INTERNAL_H
#define RUYIJS_MODEL_HEADERS_INTERNAL_H
#include "resource_network/model_headers.h"
#include "domain/resource_network/http_headers.h"

namespace ruyijs::resource_network {
namespace domain = ruyijs::domain::resource_network;
struct HeaderArena {
  size_t refs = 1, records = 0, iterators = 0, fields = 0, bytes = 0;
};
struct HeaderRecord { HeaderArena* arena; domain::HttpHeaders headers; bool immutable=false; };
struct IteratorTicket { HeaderArena* arena; };
struct HeaderDraft { std::vector<std::vector<std::string>> tuples; size_t bytes = 0; };
inline Status OutOfMemory() { return {ErrorCode::OutOfMemory, "Headers allocation failed"}; }
inline Status HeaderRange() { return {ErrorCode::Range, "Headers quota exceeded"}; }
inline Status ValidateName(std::string_view name) {
  return domain::valid_header_name(name) ? Status{} : Status{ErrorCode::Type, "Invalid HTTP header name"};
}
inline Status ValidateValue(std::string_view value) {
  return domain::valid_header_value(value) ? Status{} : Status{ErrorCode::Type, "Invalid HTTP header value"};
}
Status Commit(HeaderRecord*, domain::HttpHeaders&) noexcept;
}
#endif
