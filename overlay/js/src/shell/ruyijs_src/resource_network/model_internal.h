#pragma once
#include "resource_network/model.h"
#include "domain/resource_network/resource_bundle.h"

namespace ruyijs::resource_network {
namespace domain = ruyijs::domain::resource_network;
// The realm that owns the catalog constructs it here; a child browsing
// context's view points at that one aggregate instead and leaves the slot
// below unused. Nothing else in the product owns a second aggregate.
struct Model::Impl {
  domain::ResourceBundleAggregate owned;
  domain::ResourceBundleAggregate* shared = nullptr;
  domain::ResourceBundleAggregate& resources() noexcept { return shared ? *shared : owned; }
  const domain::ResourceBundleAggregate& resources() const noexcept {
    return shared ? *shared : owned;
  }
};
}
