#ifndef RUYIJS_BROWSER_ENVIRONMENT_SCHEMA_H
#define RUYIJS_BROWSER_ENVIRONMENT_SCHEMA_H
#include "js/CallArgs.h"
#include "browser_environment/model.h"
#include "js/RootingAPI.h"
#include "js/ErrorReport.h"
namespace ruyijs::browser_environment {
bool Error(JSContext* cx, const char* message, JSExnType type = JSEXN_TYPEERR);
// The WebIDL binding's incompatible-receiver TypeError, in Firefox's text:
// "'get webdriver' called on an object that does not implement interface
// Navigator." The member part is the callee's own name (accessors are installed
// as "get X"/"set X", methods under their name), so no table is duplicated.
bool ReceiverError(JSContext* cx, const JS::CallArgs& args, const char* iface);
bool Profile(JSContext* cx, JS::HandleObject object, bool complete,
             std::vector<Change>* changes);
bool ReadField(JSContext* cx, JS::HandleValue value, Kind kind, uint32_t field,
               const Field& descriptor, FieldValue* result);
// One validated UTF-8 string value (length and surrogate rules of every
// profile string), shared by flat fields and by nested members.
bool Text(JSContext* cx, JS::HandleValue value, std::string* result);
// Whether one string is in a "|"-separated allow-list.
bool Allowed(const std::string& text, const char* allowed);
// The two container kinds: an Object's declared members (sparse unless
// `required`) and a Map's entries (a scalar or a complete record each).
bool Members(JSContext* cx, JS::HandleObject object, const Field* fields,
             size_t count, bool required, FieldValue* result);
bool MapEntries(JSContext* cx, JS::HandleObject object, const Field& descriptor,
                FieldValue* result);
// One validated field written into the group object its owner port receives,
// and the whole merge of a group's changes into that object.
bool WriteField(JSContext* cx, JS::HandleObject group, const Field& descriptor,
                const FieldValue& value);
// The configure-only owner group named |name| (model.h kOwnerGroups), or null.
const OwnerGroupSpec* FindOwnerGroup(const std::string& name);
bool MergeGroup(JSContext* cx, JS::MutableHandleObject buffer, Kind kind,
                const Field* fields, size_t count,
                const std::vector<Change>& changes);
bool ValidURL(JSContext* cx, const std::string& value);
}
#endif
