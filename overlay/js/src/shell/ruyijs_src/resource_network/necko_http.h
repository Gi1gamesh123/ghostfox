#ifndef RUYIJS_NECKO_HTTP_H
#define RUYIJS_NECKO_HTTP_H
#include "resource_network/page_http.h"
namespace ruyijs::resource_network {
bool NeckoHttp(const std::string& method, const std::string& url, const HeaderPairs&,
               const std::string& body, HttpResult* result, uint32_t timeoutMs = 20000);
// Background start of helper sessions at Context start (necko_http.cpp).
void PrewarmNeckoSessions(size_t count) noexcept;
}
#endif
