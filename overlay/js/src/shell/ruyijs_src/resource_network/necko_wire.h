#ifndef RUYIJS_NECKO_WIRE_H
#define RUYIJS_NECKO_WIRE_H
#include "resource_network/necko_pipe.h"
#include "resource_network/page_http.h"

namespace ruyijs::resource_network {
bool WriteNeckoRequest(NeckoPipe&, const std::string& method, const std::string& url,
                       const HeaderPairs&, const std::string& body, ULONGLONG deadline);
// `*framed` reports whether one whole response or in-band error frame was
// read, that is, whether the pipe can carry the next request.
bool ReadNeckoResponse(NeckoPipe&, HttpResult*, ULONGLONG deadline, bool* framed = nullptr);
}
#endif
