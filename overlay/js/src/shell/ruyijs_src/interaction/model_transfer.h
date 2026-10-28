#pragma once
#include <string>
#include <vector>

namespace ruyijs::interaction {
struct TransferModel;
enum class TransferStatus { Ok, Limit, NoMemory };
TransferModel* NewTransferModel() noexcept;
void DeleteTransferModel(TransferModel*) noexcept;
TransferStatus TransferSet(TransferModel&, const std::string&, const std::u16string&) noexcept;
TransferStatus TransferGet(const TransferModel&, const std::string&, std::u16string*) noexcept;
TransferStatus TransferClear(TransferModel&, const std::string&) noexcept;
TransferStatus TransferTypes(const TransferModel&, std::vector<std::string>*) noexcept;
const char* TransferEffect(const TransferModel&, bool drop) noexcept;
void SetTransferEffect(TransferModel&, bool drop, const std::string&) noexcept;
}  // namespace ruyijs::interaction
