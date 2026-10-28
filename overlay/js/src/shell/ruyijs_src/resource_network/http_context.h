#pragma once
#include <string>
#include <vector>
namespace ruyijs::resource_network {
enum class Credentials { Omit, SameOrigin, Include };
struct HttpOptions {
  Credentials credentials=Credentials::Include;
  bool navigation=false;
  std::string source,site;
  const char* initiator="";
  bool cors=false;
  std::string referrer="about:client",referrerPolicy,mode;
  bool urgent=false;
  // A top-level navigation the browser itself starts (address bar, reload of
  // such a document): no initiator document, so no Referer and Sec-Fetch-Site
  // "none", with the user activation that sends Sec-Fetch-User (Firefox
  // nsHttpChannel; trace 20261021-160155 homepage and its reload).
  bool browserInitiated=false;
  bool userActivated=false;
};
struct HttpContext {std::string source,site,userAgent;double now=0;std::vector<std::string> languages;};
using HttpContextPort=bool (*)(void*,HttpContext*);
struct StorageArea;
}
