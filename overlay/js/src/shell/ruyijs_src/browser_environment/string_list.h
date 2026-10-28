#ifndef RUYIJS_BROWSER_ENVIRONMENT_STRING_LIST_H
#define RUYIJS_BROWSER_ENVIRONMENT_STRING_LIST_H
#include "js/CallArgs.h"
#include "js/TypeDecls.h"
#include <string>
#include <vector>

namespace ruyijs::browser_environment {
struct State;

// Slot ids for the DOMStringList surface installed on its prototype. One
// JSNative dispatches on the slot so the whole interface keeps one installer.
enum StringListMember : int32_t {
  kStringListLength = 700, kStringListItem, kStringListContains
};

// The realm's one DOMStringList interface: a read-only indexed list of strings
// with length/item/contains, exposed as a global constructor. The product uses
// it for Location.ancestorOrigins, and the ancestry it reports is Browser
// Environment state, so this owner lives here instead of a second environment
// layer. Instances carry no writable state of their own: their values are the
// frozen JS array the creator passed in, which is what every read goes through.
bool InstallStringList(State& state);

// Build one DOMStringList over `values`. Returns nullptr with an exception
// pending on allocation failure.
JSObject* NewStringList(State& state, const std::vector<std::string>& values);
JSObject* NewStringListArray(State& state, JS::HandleObject values);

// Installs the single ancestorOrigins accessor on the Location instance the
// factory just built. The getter reads this browsing context's ancestry from
// the Browser Environment owner and creates the list on demand.
bool InstallAncestorOrigins(State& state, JS::HandleObject location);
bool AncestorOriginsGetter(JSContext* cx, unsigned argc, JS::Value* vp);
bool StringListGetter(JSContext* cx, unsigned argc, JS::Value* vp);
bool StringListMethod(JSContext* cx, unsigned argc, JS::Value* vp);
}  // namespace ruyijs::browser_environment
#endif
