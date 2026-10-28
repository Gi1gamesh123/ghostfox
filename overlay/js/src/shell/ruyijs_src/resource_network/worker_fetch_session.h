#pragma once
#include "resource_network/async_http.h"
#include <memory>

namespace ruyijs::resource_network {
class WorkerFetchSession {
 public:
  virtual ~WorkerFetchSession()=default;
  virtual bool Request(std::string_view,std::string*) noexcept=0;
};
Status NewWorkerFetchSession(const std::shared_ptr<AsyncHttpSource>&,
                             std::shared_ptr<WorkerFetchSession>*) noexcept;
}
