#pragma once
// Blob.stream() (Web Platform) reaches the Resource Network stream factory of the
// same realm. The guard clears the port before the resource realm is destroyed.
namespace ruyijs {
namespace web_platform { class Realm; }
namespace resource_network { class Realm; }
void BindBlobStreams(web_platform::Realm& platform, resource_network::Realm& resources);
}  // namespace ruyijs
