#ifndef RUYIJS_RESOURCE_PAGE_COOKIE_STORE_H
#define RUYIJS_RESOURCE_PAGE_COOKIE_STORE_H
#include "js/CallArgs.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include <string>

namespace ruyijs { class PageHelpers; }
namespace ruyijs::resource_network {
struct State;
struct CookieChangeSet;
struct CookieEntry;

// Reserved slots of the realm's one CookieStore instance: the owning realm, the
// onchange handler and the EventTarget listener methods the installer captured
// (never a page-replaceable prototype member).
enum CookieStoreSlot { kStoreOwner, kStoreHandler, kStoreAdd, kStoreRemove, kStoreSlotCount };

// One CookieListItem object of the interface's results: {name, value} for a
// cookie and {name} only for a deletion (page_cookie_store.cpp).
JSObject* CookieItem(JSContext*, const CookieEntry&, bool withValue = true);
// The realm's document URL and cookie clock, read through the same runtime ports
// document.cookie uses (page_cookie_options.cpp).
bool CookiePageAddress(State&, std::string* url, double* now);
// One validated CookieStore.get/getAll argument list: the name to match (empty
// matches every cookie), the query URL and the clock the query stamps.
struct CookieQueryOptions { std::string name, url; double now = 0; };
// One validated write: the "Set-Cookie" text the one jar parses, the document
// URL it applies to and the clock it stamps.
struct CookieWriteOptions { std::string text, url; double now = 0; };
// Dictionary conversion for the four methods. A rejected operation carries its
// TypeError text in |error|; an empty |error| means an engine error is pending.
bool CookieQueryArguments(State&, bool requireName, const JS::CallArgs&,
                          CookieQueryOptions*, std::string* error);
bool CookieSetArguments(State&, const JS::CallArgs&, CookieWriteOptions*, std::string* error);
bool CookieDeleteArguments(State&, const JS::CallArgs&, CookieWriteOptions*, std::string* error);
// The four interface operations, each resolving or rejecting its own promise
// (page_cookie_store_methods.cpp).
bool CookieStoreGet(State&, const JS::CallArgs&);
bool CookieStoreGetAll(State&, const JS::CallArgs&);
bool CookieStoreSet(State&, const JS::CallArgs&);
bool CookieStoreDelete(State&, const JS::CallArgs&);
// Queues the trusted CookieChangeEvent for one completed assignment as a realm
// task (page_cookie_events.cpp); a no-op when nothing changed.
bool PostCookieChange(State&, const CookieChangeSet&);
// window.cookieStore (dom/webidl/CookieStore.webidl): the one CookieStore
// instance of this realm, over the cookie jar this owner already keeps.
bool InstallCookieStore(State&, PageHelpers&);
}
#endif
