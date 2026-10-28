#pragma once

#include "application/domain.h"
#include "application/dto.h"
#include "domain/resource_network/resource_bundle.h"

namespace ruyijs::application {

class NetworkInterceptAppService {
public:
    /// Add a resource to the offline bundle (Python API: ctx.add_resource())
    static void add_resource(domain::resource_network::ResourceBundleAggregate& bundle,
                             const dto::ResourceDto& dto) {
        domain::resource_network::HttpHeaders headers;
        for (const auto& [name, value] : dto.headers) {
            headers.set(name, value);
        }
        bundle.add_resource(dto.url, dto.body,
            domain::resource_network::HttpStatus(dto.status), headers);
    }

    /// Match a request URL against the bundle (used by XHR/fetch)
    static const domain::resource_network::Resource* match_request(
        domain::resource_network::ResourceBundleAggregate& bundle,
        domain::resource_network::HttpMethod method,
        const std::string& url) {
        return bundle.record_and_match(method, url);
    }
};

} // namespace ruyijs::application
