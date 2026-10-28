#pragma once
namespace ruyijs {
struct BrowsingContext;
namespace resource_network {class Realm;}
namespace execution_scheduling {class Realm;}
void BindHttpEnvironment(BrowsingContext&,resource_network::Realm&,execution_scheduling::Realm&);
}
