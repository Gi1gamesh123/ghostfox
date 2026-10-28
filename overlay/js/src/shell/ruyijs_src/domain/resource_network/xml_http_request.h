#ifndef RUYIJS_DOMAIN_XML_HTTP_REQUEST_H
#define RUYIJS_DOMAIN_XML_HTTP_REQUEST_H
#include <cstdint>

namespace ruyijs::domain::resource_network {
// Per-XHR protocol state. Resource bytes, request logging and transport remain
// with ResourceBundle and its adapter; this value owns no second catalog.
struct XmlHttpRequestState {
  int ready = 0;
  bool pending = false, asynchronous = true, credentials = false;
  // MozXMLHttpRequestParameters: an anonymous request carries no credentials.
  // mozSystem needs the systemXHR permission, which content never has.
  bool anonymous = false;
  uint32_t timeout = 0;
  uint64_t generation = 0;
  bool writable() const { return ready == 1 && !pending; }
  bool change(int next) {
    bool changed = ready != next;
    ready = next;
    return changed;
  }
  bool open(bool async) {
    ++generation;
    pending = false;
    asynchronous = async;
    return change(1);
  }
  bool begin() {
    if (!writable()) return false;
    pending = true;
    ++generation;
    return true;
  }
  bool abort() {
    if (ready == 0 || ready == 4) return false;
    ++generation;
    pending = false;
    ready = 4;
    return true;
  }
};
}
#endif
