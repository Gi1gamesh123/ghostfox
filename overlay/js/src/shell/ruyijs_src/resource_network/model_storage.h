#ifndef RUYIJS_MODEL_STORAGE_H
#define RUYIJS_MODEL_STORAGE_H
#include "resource_network/model.h"
#include <string>
#include <string_view>
#include <vector>

namespace ruyijs::resource_network {
// Opaque page store. It holds the imported platform_apis::WebStorage owners for
// localStorage and sessionStorage plus the URL-scoped CookieStore, so the
// throwing engine-independent boundary stays inside model_storage.cpp and the
// JSAPI installer sees only these noexcept entry points.
struct StorageArea;

Status NewStorage(StorageArea**) noexcept;
void DeleteStorage(StorageArea*) noexcept;

// |session| selects sessionStorage over localStorage; both are the same owner
// type with independent state, as in the source aggregate.
Status StorageSet(StorageArea*, bool session, std::string_view key,
                  std::string_view value) noexcept;
// |found| is false for a missing key, which the caller reports as null.
Status StorageGet(StorageArea*, bool session, std::string_view key,
                  std::string* value, bool* found) noexcept;
Status StorageRemove(StorageArea*, bool session, std::string_view key) noexcept;
Status StorageClear(StorageArea*, bool session) noexcept;
Status StorageLength(StorageArea*, bool session, size_t* length) noexcept;
// Insertion-ordered key, matching the source owner's key_order_.
Status StorageKeyAt(StorageArea*, bool session, size_t index,
                    std::string* key, bool* found) noexcept;

// One cookie a script-visible read returns. window.cookieStore turns it into
// the {name, value} object Firefox produces.
struct CookieEntry { std::string name, value; };
// What one assignment changed: the cookies written (name and value) and the
// cookies removed (name only), for the queued CookieChangeEvent.
struct CookieChangeSet { std::vector<CookieEntry> changed, deleted; };

// Host-only snapshot; strings preserve the jar's bytes, timestamps are Unix seconds.
struct CookieRecord {
  std::string name, value, domain, path, sameSite;
  bool hostOnly=true, secure=false, httpOnly=false, session=true;
  double expires=0, created=0;
  uint64_t order=0;
};
Status CookieSnapshot(StorageArea*, double now, std::vector<CookieRecord>*) noexcept;

// HTTP and Document use this one URL-scoped CookieStore.
Status CookieAssign(StorageArea*, const std::string& url, const std::string& text,
                    double now, bool http,const std::string& source="",bool navigation=false,
                    CookieChangeSet* changes=nullptr) noexcept;
// The cookies window.cookieStore reads for |url|, in the order CookieJoined
// joins them and with httpOnly cookies left out (|http| is false from script).
Status CookieQuery(StorageArea*, const std::string& url, double now, bool http,
                   std::vector<CookieEntry>* matches) noexcept;
Status CookieJoined(StorageArea*, const std::string& url, double now, bool http,
                    std::string* joined, const std::string& source = "",
                    bool navigation = false, bool safe = true) noexcept;
}
#endif
