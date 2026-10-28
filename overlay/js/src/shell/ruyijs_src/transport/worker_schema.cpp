#include "transport/worker_schema_internal.h"
#include <new>

namespace ruyijs::transport::worker {
Status ValidatePayload(const ControlDocument& document, Sender sender) noexcept {
  if (sender != Sender::Parent && sender != Sender::Worker) return Failure(Error::Field);
  const auto* root = ControlAccess::Root(document);
  if (!root) return Failure(Error::Envelope);
  try {
    auto type = document.Type();
    const auto& payload = (*root)["payload"];
    if (type != "hello" && type != "fatal") {
      bool parent = type == "reserve" || type == "activate" || type == "release-reservation" ||
          type == "ack-reservation" || type == "download-credit" || type == "upload-end" ||
          type == "cancel" || type == "ack-terminal" || type == "close";
      if (parent != (sender == Sender::Parent)) return Failure(Error::Field);
    }
    bool valid = schema::Handshake(type, payload, sender) || schema::Reservation(type, payload) ||
        schema::Request(type, payload) || schema::Shutdown(type, payload);
    return valid ? Status{} : Failure(Error::Field);
  } catch (const std::bad_alloc&) { return Failure(Error::Memory); }
  catch (...) { return Failure(Error::Field); }
}
}
