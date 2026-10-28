#pragma once
#include "interaction/internal.h"
#include "interaction/model_transfer.h"

namespace ruyijs::interaction {
enum TransferSlot : uint32_t { kTransferModel, kTransferItems, kTransferSlots };
extern const JSClass kTransferClass, kTransferListClass, kTransferItemClass;
extern const JSPropertySpec kTransferProperties[], kTransferListProperties[], kTransferItemProperties[];
extern const JSFunctionSpec kTransferMethods[], kTransferListMethods[], kTransferItemMethods[];
bool InstallDataTransfer(State&);
TransferModel* TransferReceiver(JSContext*, const JS::CallArgs&, JS::MutableHandleObject);
bool TransferListReceiver(JSContext*, const JS::CallArgs&, JS::MutableHandleObject);
bool TransferString(JSContext*, JS::HandleValue, std::string*);
bool TransferText(JSContext*, JS::HandleValue, std::u16string*);
bool TransferResult(JSContext*, TransferStatus);
void ObserveTransfer(JSContext*, const char* iface, const char* member, ApiAccess);
}  // namespace ruyijs::interaction
