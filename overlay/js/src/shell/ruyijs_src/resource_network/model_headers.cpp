#include "resource_network/model_headers_internal.h"
#include "domain/resource_network/response_headers.h"
#include <memory>

namespace ruyijs::resource_network {
void MakeHeadersImmutable(HeaderRecord* record) noexcept { record->immutable=true; }
bool HeadersImmutable(const HeaderRecord* record) noexcept { return record->immutable; }
Status FilterResponseHeaders(const HeaderPairs& input,bool crossOrigin,std::string_view mode,
                             bool credentialsInclude,HeaderPairs* output) noexcept {
  try {
    *output=domain::FilterResponseHeaders(input,crossOrigin,mode,credentialsInclude);
    return {};
  } catch (...) { return OutOfMemory(); }
}
Status NewArena(HeaderArena** result) noexcept {
  try { *result = new HeaderArena; return {}; } catch (...) { return OutOfMemory(); }
}
void Retain(HeaderArena* arena) noexcept { ++arena->refs; }
void Release(HeaderArena* arena) noexcept { if (arena && !--arena->refs) delete arena; }
Status NewDraft(HeaderDraft** result) noexcept {
  try { *result = new HeaderDraft; return {}; } catch (...) { return OutOfMemory(); }
}
void DeleteDraft(HeaderDraft* draft) noexcept { delete draft; }
Status DraftTuple(HeaderDraft* draft) noexcept {
  if (draft->tuples.size() >= Limits::header_fields) return HeaderRange();
  try { draft->tuples.emplace_back(); return {}; } catch (...) { return OutOfMemory(); }
}
Status DraftValue(HeaderDraft* draft, std::string_view value) noexcept {
  if (draft->tuples.empty()) return {ErrorCode::InvalidState, "Missing header tuple"};
  if (draft->tuples.back().size() >= Limits::header_fields || value.size() > Limits::header_bytes - draft->bytes)
    return HeaderRange();
  try {
    draft->tuples.back().emplace_back(value);
    draft->bytes += value.size(); return {};
  } catch (...) { return OutOfMemory(); }
}
Status ByteString(const char16_t* text, size_t length, std::string* result) noexcept {
  for (size_t i = 0; i < length; ++i)
    if (text[i] > 255) return {ErrorCode::Type, "Headers requires ByteString characters"};
  if (length > Limits::header_bytes) return HeaderRange();
  try {
    std::string candidate(length, '\0');
    for (size_t i = 0; i < length; ++i) candidate[i] = char(text[i]);
    result->swap(candidate); return {};
  } catch (...) { return OutOfMemory(); }
}
Status Commit(HeaderRecord* record, domain::HttpHeaders& candidate) noexcept {
  auto* arena = record->arena;
  const size_t fields = candidate.value_count(), bytes = candidate.bytes();
  if (fields > Limits::header_fields || bytes > Limits::header_bytes ||
      arena->fields - record->headers.value_count() + fields > Limits::arena_fields ||
      arena->bytes - record->headers.bytes() + bytes > Limits::arena_bytes) return HeaderRange();
  arena->fields = arena->fields - record->headers.value_count() + fields;
  arena->bytes = arena->bytes - record->headers.bytes() + bytes;
  record->headers.swap(candidate);
  return {};
}
Status CreateHeaders(HeaderArena* arena, HeaderDraft* draft, HeaderRecord** result) noexcept {
  try {
    domain::HttpHeaders candidate;
    for (const auto& tuple : draft->tuples) {
      if (tuple.size() != 2) return {ErrorCode::Type, "Headers entry must contain exactly two items"};
      const auto value = domain::trim_header_value(tuple[1]);
      auto status = ValidateName(tuple[0]); if (status.code != ErrorCode::None) return status;
      status = ValidateValue(value); if (status.code != ErrorCode::None) return status;
      candidate.append(tuple[0], std::string(value));
    }
    if (arena->records >= Limits::header_records) return HeaderRange();
    auto record = std::make_unique<HeaderRecord>(); record->arena = arena;
    auto status = Commit(record.get(), candidate); if (status.code != ErrorCode::None) return status;
    ++arena->records; Retain(arena); *result = record.release(); return {};
  } catch (...) { return OutOfMemory(); }
}
void DeleteHeaders(HeaderRecord* record) noexcept {
  if (!record) return;
  auto* arena = record->arena;
  --arena->records; arena->fields -= record->headers.value_count(); arena->bytes -= record->headers.bytes();
  delete record; Release(arena);
}
Status NewTicket(HeaderRecord* record, IteratorTicket** result) noexcept {
  if (record->arena->iterators >= Limits::header_iterators) return HeaderRange();
  try {
    *result = new IteratorTicket{record->arena};
    ++record->arena->iterators; Retain(record->arena); return {};
  } catch (...) { return OutOfMemory(); }
}
void DeleteTicket(IteratorTicket* ticket) noexcept {
  if (!ticket) return;
  auto* arena = ticket->arena; --arena->iterators;
  delete ticket; Release(arena);
}
}
