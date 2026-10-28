// Stateless assembly bridge to the existing resource and Interaction owners.
#pragma once
namespace ruyijs {
struct BrowsingContext;
namespace resource_network { class Realm; }
void BindStylingLoads(BrowsingContext&, resource_network::Realm&);
class StylingLoadBinding final {
 public:
  StylingLoadBinding(BrowsingContext&, resource_network::Realm&);
  ~StylingLoadBinding();
 private:
  BrowsingContext& owner_;
};
}
