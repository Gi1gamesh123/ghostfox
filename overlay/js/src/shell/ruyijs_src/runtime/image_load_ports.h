#ifndef RUYIJS_RUNTIME_IMAGE_LOAD_PORTS_H
#define RUYIJS_RUNTIME_IMAGE_LOAD_PORTS_H
namespace ruyijs::resource_network { class AsyncHttp; }
namespace ruyijs {
struct BrowsingContext;
// Binds the realm's image fetch port to the Context's asynchronous HTTP owner.
void BindImageLoads(BrowsingContext&, resource_network::AsyncHttp&);
}
#endif
