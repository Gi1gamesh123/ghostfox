#include "resource_network/necko_wire.h"
#include <cmath>

namespace ruyijs::resource_network {
namespace {
constexpr uint32_t kBodyLimit = Limits::body_bytes, kFieldLimit = 65536, kHeaderLimit = 256;
bool WriteText(NeckoPipe& pipe, const std::string& text, ULONGLONG deadline) {
  if (text.size() > kFieldLimit) return false;
  const uint32_t size = uint32_t(text.size());
  return pipe.Write(&size, 4, deadline) && pipe.Write(text.data(), size, deadline);
}
bool ReadText(NeckoPipe& pipe, std::string* text, ULONGLONG deadline) {
  uint32_t size = 0;
  if (!pipe.Read(&size, 4, deadline) || size > kFieldLimit) return false;
  text->assign(size, '\0');
  return pipe.Read(text->data(), size, deadline);
}
}
bool WriteNeckoRequest(NeckoPipe& pipe, const std::string& method, const std::string& url,
                       const HeaderPairs& headers, const std::string& body, ULONGLONG deadline) {
  if (headers.size() > kHeaderLimit || body.size() > kBodyLimit) return false;
  size_t bytes = 0;
  for (const auto& field : headers) {
    if (field.first.size() > kFieldLimit || field.second.size() > kFieldLimit) return false;
    bytes += field.first.size() + field.second.size();
    if (bytes > kFieldLimit) return false;
  }
  if (!WriteText(pipe, method, deadline) || !WriteText(pipe, url, deadline)) return false;
  const uint32_t count = uint32_t(headers.size()), size = uint32_t(body.size());
  if (!pipe.Write(&count, 4, deadline)) return false;
  for (const auto& field : headers)
    if (!WriteText(pipe, field.first, deadline) || !WriteText(pipe, field.second, deadline)) return false;
  return pipe.Write(&size, 4, deadline) && pipe.Write(body.data(), size, deadline);
}
bool ReadNeckoResponse(NeckoPipe& pipe, HttpResult* result, ULONGLONG deadline, bool* framed) {
  uint32_t status = 0, count = 0, size = 0;
  if (framed) *framed = false;
  if (!pipe.Read(&status, 4, deadline)) return false;
  if (!status) {
    if (ReadText(pipe, &result->error, deadline) && framed) *framed = true;
    return false;
  }
  if (status < 100 || status > 599 || !pipe.Read(&count, 4, deadline) || count > kHeaderLimit)
    return false;
  result->status = int(status);
  result->kind = BodyKind::Bytes;
  size_t bytes = 0;
  for (uint32_t index = 0; index < count; ++index) {
    std::string name, value;
    if (!ReadText(pipe, &name, deadline) || !ReadText(pipe, &value, deadline)) return false;
    bytes += name.size() + value.size();
    if (bytes > kFieldLimit) return false;
    result->headers.emplace_back(std::move(name), std::move(value));
  }
  if (!pipe.Read(&size, 4, deadline) || size > kBodyLimit) return false;
  result->body.assign(size, '\0');
  uint32_t version=0;
  if(!pipe.Read(result->body.data(),size,deadline)||!pipe.Read(&version,4,deadline)||
     (version!=0x31545252&&version!=0x32545252)||
     !pipe.Read(&result->encodedBytes,8,deadline)||!pipe.Read(&result->decodedBytes,8,deadline)||
     !ReadText(pipe,&result->protocol,deadline)||result->protocol.size()>32||result->decodedBytes!=size)return false;
  if(version==0x32545252) {
    if(!pipe.Read(&result->requestTick,8,deadline)||!pipe.Read(&result->responseTick,8,deadline)||
       !pipe.Read(&result->headersTick,8,deadline))return false;
    const double a=result->requestTick,b=result->responseTick,c=result->headersTick;
    if(!std::isfinite(a)||!std::isfinite(b)||!std::isfinite(c)||a<0||b<0||c<0||
       (b&&(!a||b<a))||(c&&(!a||c<a||c<b)))return false;
  }
  result->transferred=true;
  if (framed) *framed = true;
  return true;
}
}
