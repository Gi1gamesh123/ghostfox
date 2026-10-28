#pragma once
#include "resource_network/model_cache.h"
#include <memory>

namespace ruyijs::resource_network {
class CacheSession {
 public:
  virtual ~CacheSession()=default;
  virtual bool Request(std::string_view,std::string*) noexcept=0;
};
Status NewCacheSession(Model&,std::string_view,std::shared_ptr<CacheSession>*) noexcept;
}
