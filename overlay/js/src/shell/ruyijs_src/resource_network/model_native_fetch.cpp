// Native allocation/copy boundary only. JS rooting and calls stay with adapters.
#include "resource_network/native_fetch_internal.h"
#include <utility>

namespace ruyijs::resource_network {
Status PrepareNativeFetch(const RequestDraft& request,const HttpContext& context,
    std::shared_ptr<NativeFetchRecord>* result,HttpOptions* options) noexcept {
  try {
    auto record=std::make_shared<NativeFetchRecord>();
    record->method=request.method;record->url=request.url;
    record->mode=request.mode;record->credentialsInclude=request.credentials=="include";
    HttpOptions staged;
    staged.initiator="fetch";
    staged.source=context.source;staged.site=context.site;
    staged.credentials=request.credentials=="omit"?Credentials::Omit:
      request.credentials=="include"?Credentials::Include:Credentials::SameOrigin;
    staged.cors=request.mode=="cors";staged.mode=request.mode;
    staged.referrer=request.referrer;staged.referrerPolicy=request.referrer_policy;
    *options=std::move(staged);*result=std::move(record);return {};
  } catch (...) { return {ErrorCode::OutOfMemory,"Native fetch admission allocation failed"}; }
}
}
