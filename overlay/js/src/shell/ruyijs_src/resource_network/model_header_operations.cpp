#include "resource_network/model_headers_internal.h"

namespace ruyijs::resource_network {
Status Mutate(HeaderRecord* record, Mutation operation, std::string_view name, std::string_view raw) noexcept {
  auto status = ValidateName(name); if (status.code != ErrorCode::None) return status;
  const auto value = domain::trim_header_value(raw);
  if (operation != Mutation::Delete) {
    status = ValidateValue(value); if (status.code != ErrorCode::None) return status;
  }
  if (record->immutable) return {ErrorCode::Type, "Headers are immutable"};
  try {
    auto candidate = record->headers;
    if (operation == Mutation::Delete) candidate.erase(std::string(name));
    else if (operation == Mutation::Set) candidate.set(std::string(name), std::string(value));
    else candidate.append(std::string(name), std::string(value));
    return Commit(record, candidate);
  } catch (...) { return OutOfMemory(); }
}
Status Get(HeaderRecord* record, std::string_view name, std::string* value, bool* found) noexcept {
  auto status = ValidateName(name); if (status.code != ErrorCode::None) return status;
  try {
    const std::string key(name);
    *found = record->headers.has(key);
    *value = record->headers.combined_get(key); return {};
  } catch (...) { return OutOfMemory(); }
}
Status Has(HeaderRecord* record, std::string_view name, bool* found) noexcept {
  auto status = ValidateName(name); if (status.code != ErrorCode::None) return status;
  try { *found = record->headers.has(std::string(name)); return {}; }
  catch (...) { return OutOfMemory(); }
}
Status Cookies(HeaderRecord* record, std::vector<std::string>* result) noexcept {
  try { *result = record->headers.get_all("set-cookie"); return {}; }
  catch (...) { return OutOfMemory(); }
}
Status Entry(HeaderRecord* record, size_t index, std::string* key, std::string* value, bool* found) noexcept {
  try {
    const auto projection = record->headers.sorted_combined();
    *found = index < projection.size();
    if (*found) { *key = projection[index].first; *value = projection[index].second; }
    return {};
  } catch (...) { return OutOfMemory(); }
}
Status SnapshotHeaders(HeaderRecord* record,HeaderPairs* pairs) noexcept {
  try {
    HeaderPairs result;
    for (size_t index=0;;++index) {
      std::string name,value;bool found=false;
      auto status=Entry(record,index,&name,&value,&found);
      if (status.code!=ErrorCode::None) return status;
      if (!found) break;
      result.emplace_back(std::move(name),std::move(value));
    }
    pairs->swap(result);return {};
  } catch (...) {return OutOfMemory();}
}

}
