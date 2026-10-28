#include "runtime/blob_stream_port.h"
#include "resource_network/realm.h"
#include "web_platform/realm.h"

namespace ruyijs {
void BindBlobStreams(web_platform::Realm& platform, resource_network::Realm& resources) {
  platform.SetByteStreamPort([](void* context, std::string_view bytes, JS::MutableHandleObject stream) {
    return static_cast<resource_network::Realm*>(context)->NewByteStream(bytes, stream);
  }, &resources);
}
}  // namespace ruyijs
