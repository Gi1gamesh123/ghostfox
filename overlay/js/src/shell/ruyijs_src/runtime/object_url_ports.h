#ifndef RUYIJS_RUNTIME_OBJECT_URL_PORTS_H
#define RUYIJS_RUNTIME_OBJECT_URL_PORTS_H
namespace ruyijs::resource_network { class Realm; }
namespace ruyijs {
struct BrowsingContext;
void BindObjectUrlCreation(BrowsingContext& target);
void BindObjectUrlWorkers(BrowsingContext& target);
void BindObjectUrlNetwork(BrowsingContext& target, resource_network::Realm& resources);
}
#endif
