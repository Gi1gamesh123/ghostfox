// Allocation boundary: no engine values or callbacks cross this codec layer.
#include "web_media/image_codec.h"
#include <new>
namespace ruyijs::web_media {
bool ImageAppendBytes(std::vector<uint8_t>* target, const uint8_t* bytes, size_t size) {
  if (size > domain::web_media::kEncodedImageLimit - target->size()) return false;
  try { if (size) target->insert(target->end(), bytes, bytes + size); return true; }
  catch (const std::bad_alloc&) { return false; }
}
}
