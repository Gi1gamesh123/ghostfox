#include "resource_network/model_storage.h"
#include "domain/platform_apis/platform_apis.h"
#include "domain/resource_network/cookie_parse.h"
#include <cmath>
#include <new>

namespace ruyijs::resource_network {
namespace domain = ruyijs::domain::platform_apis;

// The imported WebStorage owners hold every Web Storage key; this adapter adds
// no second copy and no business rule of its own. Only WebStorage is wired from
// the imported header: GlParameters and PlatformApisAggregate stay unused
// source for the later WebGL and canvas checkpoints.
struct StorageArea {
  domain::WebStorage local, session;
  ruyijs::domain::resource_network::CookieStore cookie;
  domain::WebStorage& Pick(bool wantSession) { return wantSession ? session : local; }
};

static Status Ok() { return Status{}; }

Status NewStorage(StorageArea** area) noexcept {
  *area = new (std::nothrow) StorageArea;
  if (!*area) return Status{ErrorCode::OutOfMemory, "Storage allocation failed"};
  return Ok();
}

void DeleteStorage(StorageArea* area) noexcept { delete area; }

Status StorageSet(StorageArea* area, bool session, std::string_view key,
                  std::string_view value) noexcept {
  area->Pick(session).set_item(std::string(key), std::string(value));
  return Ok();
}

Status StorageGet(StorageArea* area, bool session, std::string_view key,
                  std::string* value, bool* found) noexcept {
  auto& storage = area->Pick(session);
  std::string name(key);
  *found = storage.has_item(name);
  // The source owner returns "" for a missing key, so presence decides null.
  *value = *found ? storage.get_item(name) : std::string();
  return Ok();
}

Status StorageRemove(StorageArea* area, bool session, std::string_view key) noexcept {
  area->Pick(session).remove_item(std::string(key));
  return Ok();
}

Status StorageClear(StorageArea* area, bool session) noexcept {
  area->Pick(session).clear();
  return Ok();
}

Status StorageLength(StorageArea* area, bool session, size_t* length) noexcept {
  *length = area->Pick(session).length();
  return Ok();
}

Status StorageKeyAt(StorageArea* area, bool session, size_t index,
                    std::string* key, bool* found) noexcept {
  auto& storage = area->Pick(session);
  *found = index < storage.length();
  *key = *found ? storage.key_at(index) : std::string();
  return Ok();
}

Status CookieAssign(StorageArea* area, const std::string& url, const std::string& text,
                    double now, bool http,const std::string& source,bool navigation,
                    CookieChangeSet* changes) noexcept {
  try {
    ruyijs::domain::resource_network::CookieChanges collected;
    area->cookie.Assign(url,text,now,http,source,navigation,changes?&collected:nullptr);
    if (changes) {
      for (const auto& pair : collected.changed) changes->changed.push_back({pair.name, pair.value});
      for (const auto& pair : collected.deleted) changes->deleted.push_back({pair.name, {}});
    }
    return Ok();
  } catch (...) {return {ErrorCode::OutOfMemory,"Cookie allocation failed"};}
}

Status CookieQuery(StorageArea* area, const std::string& url, double now, bool http,
                   std::vector<CookieEntry>* matches) noexcept {
  try {
    for (const auto& pair : area->cookie.Matches(url,now,http))
      matches->push_back({pair.name, pair.value});
    return Ok();
  } catch (...) {return {ErrorCode::OutOfMemory,"Cookie allocation failed"};}
}

Status CookieJoined(StorageArea* area, const std::string& url, double now, bool http,
                    std::string* joined, const std::string& source, bool navigation, bool safe) noexcept {
  try {*joined=area->cookie.Joined(url,now,http,source,navigation,safe);return Ok();}
  catch (...) {return {ErrorCode::OutOfMemory,"Cookie allocation failed"};}
}

Status CookieSnapshot(StorageArea* area, double now, std::vector<CookieRecord>* result) noexcept {
  try {
    std::vector<CookieRecord> records;
    for (const auto& cookie : area->cookie.Snapshot(now)) {
      using SameSite = ruyijs::domain::resource_network::CookieSameSite;
      const char* site = cookie.sameSite == SameSite::Lax ? "Lax" :
          cookie.sameSite == SameSite::Strict ? "Strict" :
          cookie.sameSite == SameSite::None ? "None" : "Default";
      const bool session = !std::isfinite(cookie.expires);
      records.push_back({cookie.name, cookie.value, cookie.domain, cookie.path, site,
          cookie.hostOnly, cookie.secure, cookie.httpOnly, session,
          session ? 0 : cookie.expires, cookie.created, cookie.order});
    }
    *result = std::move(records);
    return Ok();
  } catch (...) {return {ErrorCode::OutOfMemory,"Cookie snapshot allocation failed"};}
}
}
