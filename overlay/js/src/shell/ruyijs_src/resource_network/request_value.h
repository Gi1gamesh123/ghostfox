#pragma once
#include "resource_network/model.h"

namespace ruyijs::resource_network {
// One request as `new Request` and the fetch entry share it: the resolved URL,
// the normalized method, the converted headers, the raw body bytes and the
// string members of the init dictionary. The AbortSignal stays a JS value
// because it is an interface object, not resource state.
struct RequestDraft {
  std::string method = "GET", url, body;
  BodyKind kind = BodyKind::Text;
  bool nullBody = true;
  HeaderPairs headers;
  std::string mode = "cors", credentials = "same-origin", cache = "default",
              redirect = "follow", referrer = "about:client", referrer_policy;
  std::u16string integrity;
  bool keepalive = false;
};

}
