#include "web_messaging/state.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::web_messaging {
namespace {
constexpr uint32_t kPortTag = JS_SCTAG_USER_MIN;
struct Transferred { std::shared_ptr<Endpoint> endpoint; };
void Error(JSContext*, uint32_t, void* closure, const char*) {
  static_cast<Message*>(closure)->error = true;
}
bool CanTransfer(JSContext*, JS::HandleObject object, bool* sameProcess, void* closure) {
  auto& message = *static_cast<Message*>(closure);
  Record* record = Unwrap(object);
  if (!record || record->closed || !record->endpoint || record->endpoint.get() == message.excluded) return false;
  *sameProcess = true; return true;
}
bool WriteTransfer(JSContext*, JS::HandleObject object, void* closure, uint32_t* tag,
                   JS::TransferableOwnership* ownership, void** content, uint64_t* extra) {
  auto& message = *static_cast<Message*>(closure);
  Record* record = Unwrap(object);
  if (!record || record->closed || !record->endpoint || record->endpoint.get() == message.excluded) return false;
  auto transfer = std::make_unique<Transferred>();
  transfer->endpoint = std::move(record->endpoint);
  auto& endpoint = *transfer->endpoint;
  if (&endpoint == message.recipient) message.recipientTransferred = true;
  endpoint.receiver = nullptr; endpoint.enabled = false; endpoint.scheduled = false;
  *tag = kPortTag; *ownership = JS::SCTAG_TMO_CUSTOM;
  *content = transfer.release(); *extra = 0;
  return true;
}
bool ReadTransfer(JSContext* cx, JSStructuredCloneReader*, const JS::CloneDataPolicy&,
                  uint32_t tag, void* content, uint64_t, void* closure,
                  JS::MutableHandleObject output) {
  if (tag != kPortTag) return false;
  auto& message = *static_cast<Message*>(closure);
  auto* transfer = static_cast<Transferred*>(content);
  output.set(NewPort(*message.destination, transfer->endpoint));
  if (!output) return false;
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, message.ports, &length) ||
      !JS_DefineElement(cx, message.ports, length, output, JSPROP_ENUMERATE)) return false;
  delete transfer;
  return true;
}
void FreeTransfer(uint32_t tag, JS::TransferableOwnership, void* content, uint64_t, void*) {
  if (tag != kPortTag) return;
  auto* transfer = static_cast<Transferred*>(content);
  Disconnect(*transfer->endpoint);
  delete transfer;
}
}
const JSStructuredCloneCallbacks cloneCallbacks = {
  nullptr, nullptr, Error, ReadTransfer, WriteTransfer, FreeTransfer, CanTransfer};
}
