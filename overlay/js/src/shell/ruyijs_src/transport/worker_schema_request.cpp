#include "transport/worker_schema_internal.h"

namespace ruyijs::transport::worker::schema {
bool Request(std::string_view type, const Value& p) {
  if (type == "upload-credit" || type == "download-credit")
    return Fields(p, {"key", "grantThrough", "ackedThrough", "stop"}) && Key(p["key"]) &&
        Watermark(p["grantThrough"]) && Watermark(p["ackedThrough"]) && p["stop"].isBool();
  if (type == "upload-end")
    return Fields(p, {"key", "lastSeq", "totalBytes"}) && Key(p["key"]) &&
        Watermark(p["lastSeq"]) && Number(p["totalBytes"], 0, 16 * 1024 * 1024);
  if (type == "headers")
    return Fields(p, {"key", "status", "version", "orderedHeaders", "tls", "attempt",
                      "bodyEncoding", "headerRepresentation"}) && Key(p["key"]) &&
        Number(p["status"], 100, 599) && OneOf(p["version"], {"http/1.1", "h2", "h3"}) &&
        Headers(p["orderedHeaders"]) && Tls(p["tls"]) && Number(p["attempt"], 1, 3) &&
        OneOf(p["bodyEncoding"], {"decoded"}) && OneOf(p["headerRepresentation"], {"encoded"});
  if (type == "body-end")
    return Fields(p, {"key", "lastSeq", "encodedBytes", "decodedBytes"}) && Key(p["key"]) &&
        Watermark(p["lastSeq"]) && Number(p["encodedBytes"], 0, 16 * 1024 * 1024) &&
        Number(p["decodedBytes"], 0, 16 * 1024 * 1024);
  if (type == "cancel")
    return Fields(p, {"key", "reason", "message", "lastUploadSeq"}) && Key(p["key"]) &&
        Reason(p["reason"]) && TextSize(p["message"], 0, 512) && Watermark(p["lastUploadSeq"]);
  if (type == "terminal") {
    if (!Fields(p, {"key", "outcome", "error", "lastDownloadSeq"}) || !Key(p["key"]) ||
        !OneOf(p["outcome"], {"complete", "error", "cancelled"}) || !Watermark(p["lastDownloadSeq"]))
      return false;
    if (Text(p["outcome"]) == "complete") return p["error"].isNull();
    return ErrorValue(p["error"], false) && (Text(p["outcome"]) != "cancelled" ||
        OneOf(p["error"]["kind"], {"Cancelled", "DeadlineExceeded"}));
  }
  if (type == "released")
    return Fields(p, {"key", "leaseClosed", "interrupts", "localResourcesReleased",
                      "lastUploadAck", "lastDownloadSeq"}) && Key(p["key"]) &&
        Boolean(p["leaseClosed"], true) && Number(p["interrupts"], 0, 0) &&
        Boolean(p["localResourcesReleased"], true) && Watermark(p["lastUploadAck"]) &&
        Watermark(p["lastDownloadSeq"]);
  return type == "ack-terminal" && Fields(p, {"key", "disposition"}) && Key(p["key"]) &&
      OneOf(p["disposition"], {"accepted", "discarded"});
}
bool Shutdown(std::string_view type, const Value& p) {
  if (type == "close") return Fields(p, {"reason"}) && Reason(p["reason"]);
  if (type == "fatal") return Fields(p, {"error"}) && ErrorValue(p["error"], false);
  if (type != "closed" || !Fields(p, {"tickets", "active", "queued", "connections",
                                     "resolverActive", "credits", "spools"})) return false;
  for (const auto& value : p) if (!Number(value, 0, 0)) return false;
  return true;
}
}
