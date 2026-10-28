#include "runtime/styling_load_ports.h"
#include "runtime/browsing_context.h"
#include "resource_network/page_http.h"
#include "resource_network/async_http.h"
#include <memory>
namespace ruyijs {
void BindStylingLoads(BrowsingContext& owner, resource_network::Realm& resources) {
  owner.styling->SetLoadPort([](void* context,const web_styling::StyleLoadRequest& request,
                                web_styling::StyleLoadResult* result) {
    resource_network::HeaderPairs headers;
    if (request.cors) headers.emplace_back("Origin",request.origin);
    resource_network::HttpResult http;
    resource_network::HttpOptions options;options.initiator="link";options.cors=request.cors;
    options.source=request.referer;
    const bool ok=resource_network::PerformHttp(*static_cast<resource_network::Model*>(context),
                                                "GET",request.url,headers,"",&http,resource_network::RedirectMode::Follow,options);
    result->status=http.status;result->body=std::move(http.body);
    result->headers=std::move(http.headers);result->error=std::move(http.error);
    return ok;
  },&resources.Resources());
  // The same request on the document's own asynchronous source (its Referer,
  // cookies and Resource Timing), so sheets load in parallel off the JS thread.
  owner.styling->SetLoadStartPort([](void* context,const web_styling::StyleLoadRequest& request,
                                     web_styling::StyleLoadDone done) {
    auto source=static_cast<resource_network::Realm*>(context)->AsyncSource();
    if (!source) return false;
    resource_network::HeaderPairs headers;
    if (request.cors) headers.emplace_back("Origin",request.origin);
    resource_network::HttpOptions options;options.initiator="link";options.cors=request.cors;
    options.source=request.referer;
    auto holder=std::make_shared<web_styling::StyleLoadDone>(std::move(done));
    const std::string method="GET",body;
    resource_network::AsyncHttpTicket ticket;
    return resource_network::StartAsyncHttp(source,{method,request.url,headers,body,
        resource_network::RedirectMode::Follow,options},{holder,[](void* target,
        resource_network::AsyncHttpResult&& response) noexcept {
          web_styling::StyleLoadResult result;
          result.fetched=response.outcome==resource_network::AsyncHttpOutcome::Http && response.ok;
          result.status=response.response.status;result.body=std::move(response.response.body);
          result.headers=std::move(response.response.headers);result.error=std::move(response.response.error);
          (*static_cast<web_styling::StyleLoadDone*>(target))(std::move(result));
        }},&ticket).code==resource_network::ErrorCode::None;
  },&resources);
  owner.styling->SetLoadEventPort([](void* context,JS::HandleObject target,const char* type) {
    return static_cast<interaction::Realm*>(context)->Fire(target,type,false,false);
  },owner.interaction.get());
}
StylingLoadBinding::StylingLoadBinding(BrowsingContext& owner,resource_network::Realm& resources)
    : owner_(owner) { BindStylingLoads(owner,resources); }
StylingLoadBinding::~StylingLoadBinding() {
  owner_.styling->SetLoadPort(nullptr,nullptr);
  owner_.styling->SetLoadStartPort(nullptr,nullptr);
  owner_.styling->SetLoadEventPort(nullptr,nullptr);
}
}
