// Original Body byte output, with native allocation bounded at the model seam.
#include "resource_network/model_body_bytes.h"

namespace ruyijs::resource_network {
Status AppendBodyBytes(std::string* output,const uint8_t* data,size_t length) noexcept {
  if (output->size()>Limits::body_bytes || length>Limits::body_bytes-output->size())
    return {ErrorCode::Range,"Body exceeds the 8 MiB byte limit"};
  try {if (length) output->append(reinterpret_cast<const char*>(data),length);return {};}
  catch (...) {return {ErrorCode::OutOfMemory,"Body allocation failed"};}
}
Status CopyBodyBytes(const std::string& input,std::string* output) noexcept {
  try {*output=input;return {};}
  catch (...) {return {ErrorCode::OutOfMemory,"Body allocation failed"};}
}
}
