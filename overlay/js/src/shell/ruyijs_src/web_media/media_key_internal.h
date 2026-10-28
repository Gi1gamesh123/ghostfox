#ifndef RUYIJS_WEB_MEDIA_MEDIA_KEY_INTERNAL_H
#define RUYIJS_WEB_MEDIA_MEDIA_KEY_INTERNAL_H
#include "web_media/media_keys.h"
#include "runtime/native_constructor_trace.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include <limits>

namespace ruyijs::web_media {
enum SessionSlot { SessionId, SessionExpiration, SessionClosed, SessionStatuses,
                   SessionError, SessionStatusHandler, SessionMessageHandler,
                   SessionOwner, SessionSlotCount };
enum StatusSlot { StatusEntries, StatusSlotCount };
enum AccessSlot { AccessKeySystem, AccessConfiguration, AccessSlotCount };
enum KeysSlot { KeysKeySystem, KeysSlotCount };
enum ErrorSlot { ErrorSystemCode, ErrorSlotCount };

bool PromiseUndefined(JSContext*, JS::MutableHandleValue);
bool PromiseValue(JSContext*, JS::HandleValue, JS::MutableHandleValue);
bool EmptyIterator(JSContext*, const char*, JS::MutableHandleValue);
bool InstallMediaKeySession(State&);
bool InstallMediaKeyStatusMap(State&);
bool InstallMediaKeySystemAccess(State&);
bool InstallMediaKeysObject(State&);
bool NewMediaKeySession(State&, JS::MutableHandleObject);
bool NewMediaKeys(State&, JS::HandleString, JS::MutableHandleObject);
}
#endif
