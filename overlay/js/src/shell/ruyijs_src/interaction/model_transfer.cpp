#include "interaction/model_transfer.h"
#include "domain/interaction/data_transfer.h"
#include <new>

namespace ruyijs::interaction {
struct TransferModel { ruyijs::domain::interaction::DataTransferData data; };
TransferModel* NewTransferModel() noexcept {
  try { return new TransferModel; } catch (const std::bad_alloc&) { return nullptr; }
}
void DeleteTransferModel(TransferModel* model) noexcept { delete model; }
TransferStatus TransferSet(TransferModel& m, const std::string& format,
                            const std::u16string& value) noexcept {
  try { return m.data.Set(format, value) ? TransferStatus::Ok : TransferStatus::Limit; }
  catch (const std::bad_alloc&) { return TransferStatus::NoMemory; }
}
TransferStatus TransferGet(const TransferModel& m, const std::string& format,
                            std::u16string* value) noexcept {
  try {
    const auto found = m.data.Get(format);
    if (found) *value = *found; else value->clear();
    return TransferStatus::Ok;
  } catch (const std::bad_alloc&) { return TransferStatus::NoMemory; }
}
TransferStatus TransferClear(TransferModel& m, const std::string& format) noexcept {
  try { m.data.Clear(format); return TransferStatus::Ok; }
  catch (const std::bad_alloc&) { return TransferStatus::NoMemory; }
}
TransferStatus TransferTypes(const TransferModel& m, std::vector<std::string>* result) noexcept {
  try {
    result->clear();
    result->reserve(m.data.formats().size());
    for (const auto& entry : m.data.formats()) result->push_back(entry.first);
    return TransferStatus::Ok;
  } catch (const std::bad_alloc&) { return TransferStatus::NoMemory; }
}
const char* TransferEffect(const TransferModel& m, bool drop) noexcept {
  return drop ? m.data.Drop() : m.data.Effect();
}
void SetTransferEffect(TransferModel& m, bool drop, const std::string& value) noexcept {
  if (drop) m.data.SetDrop(value); else m.data.SetEffect(value);
}
}  // namespace ruyijs::interaction
