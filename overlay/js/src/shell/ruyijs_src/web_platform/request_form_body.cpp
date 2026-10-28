#include "web_platform/request_body.h"
#include "web_platform/form_data_internal.h"
#include "mozilla/RandomNum.h"

namespace ruyijs::web_platform {
std::string NormalizeFormLines(const std::string& value) {
  std::string out;
  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '\r') {
      if (i + 1 < value.size() && value[i + 1] == '\n') ++i;
      out += "\r\n";
    } else if (value[i] == '\n') out += "\r\n";
    else out += value[i];
  }
  return out;
}
namespace {
std::string Quote(const std::string& value) {
  std::string out;
  for (char c : value) {
    if (c == '"') out += "%22";
    else if (c == '\r') out += "%0D";
    else if (c == '\n') out += "%0A";
    else out += c;
  }
  return out;
}
bool Append(RequestBody* out, const std::string& text) {
  if (text.size() > kRequestBodyLimit - out->bytes.size()) {
    out->tooLarge = true;
    return false;
  }
  out->bytes += text;
  return true;
}
}
bool ReadFormRequestBody(JSContext* cx, JS::HandleObject form, RequestBody* out) {
  uint8_t random[16];
  if (!mozilla::GenerateRandomBytesFromOS(random, sizeof(random)))
    return WebKit::Fail(cx, "Multipart boundary generation failed");
  std::string boundary = "---------------------------";
  for (uint8_t byte : random) {
    boundary += "0123456789abcdef"[byte >> 4];
    boundary += "0123456789abcdef"[byte & 15];
  }
  out->contentType = "multipart/form-data; boundary=" + boundary;
  JS::RootedObject list(cx);
  uint32_t count = 0;
  if (!FormDataList(cx, form, &list) ||
      !form_data_internal::Length(cx, list, &count)) return false;
  for (uint32_t index = 0; index < count; ++index) {
    JS::RootedObject pair(cx);
    JS::RootedValue name(cx), value(cx);
    std::string key;
    if (!form_data_internal::Entry(cx, list, index, &pair) ||
        !form_data_internal::Pair(cx, pair, &name, &value) ||
        !ReadRequestText(cx, name, &key)) return false;
    if (!Append(out, "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" +
                     Quote(NormalizeFormLines(key)) + "\"")) return true;
    if (value.isObject()) {
      JS::RootedObject file(cx, &value.toObject());
      JS::RootedValue filename(cx, JS::GetReservedSlot(file, kFileSlotName));
      std::string label;
      RequestBody body;
      if (!ReadRequestText(cx, filename, &label) || !ReadRequestBody(cx, value, &body)) return false;
      if (body.tooLarge) { out->tooLarge = true; return true; }
      if (!Append(out, "; filename=\"" + Quote(label) + "\"\r\nContent-Type: " +
                       (body.contentType.empty() ? "application/octet-stream" : body.contentType) +
                       "\r\n\r\n") || !Append(out, body.bytes)) return true;
    } else {
      std::string text;
      if (!ReadRequestText(cx, value, &text)) return false;
      if (!Append(out, "\r\n\r\n") || !Append(out, NormalizeFormLines(text))) return true;
    }
    if (!Append(out, "\r\n")) return true;
  }
  Append(out, "--" + boundary + "--\r\n");
  return true;
}
}
