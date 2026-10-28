#ifndef RUYIJS_BROWSER_ENVIRONMENT_NAVIGATION_API_H
#define RUYIJS_BROWSER_ENVIRONMENT_NAVIGATION_API_H
// The Navigation API over this owner's one session history
// (docs/owners/BROWSER_ENVIRONMENT.md, 2026-10-21).
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "browser_environment/state.h"
#include "browser_environment/history.h"
#include "runtime/web_kit.h"

namespace ruyijs::interaction { class Realm; }
namespace ruyijs::browser_environment {
enum NavigationEntrySlot : uint32_t {
  kNavEntryId = 0, kNavEntryRevision, kNavEntryUrl, kNavEntryKey, kNavEntryUuid, kNavEntrySame,
  kNavEntryOnDispose, kNavEntrySlots
};
enum NavigationDestinationSlot : uint32_t {
  kNavDestUrl = 0, kNavDestKey, kNavDestUuid, kNavDestIndex, kNavDestSame, kNavDestState, kNavDestSlots
};
struct NavigationState {
  NavigationState(State& env, interaction::Realm& events);
  State& env;
  interaction::Realm& events;
  WebKit kit;
  WebFace navigation, entry, activation, transition, destination;
  JS::PersistentRootedObject instance, activationObject, entries;  // entries: Map "id:rev:url" -> entry
  JS::PersistentRootedValue navigateEvent, changeEvent, abortController;
  JS::PersistentRootedValue handlers[4];  // navigate, navigatesuccess, navigateerror, currententrychange
  uint64_t activationDocument = 0;                          // entry id activationObject was built for
  std::map<uint64_t, std::string> keys;                      // entry id -> key (UUID)
  std::map<std::pair<uint64_t, uint64_t>, std::string> ids;  // (id, revision) -> id (UUID)
};
extern const JSClass kNavigationClass, kNavigationEntryClass, kNavigationActivationClass,
    kNavigationTransitionClass, kNavigationDestinationClass;

// navigation_install.cpp: Navigation, the entry interfaces and window.navigation.
bool InstallNavigation(State& state, interaction::Realm& events);
bool NavigationActivationValue(NavigationState& n, const std::vector<Model::HistoryView>& views, int32_t index,
                               JS::MutableHandleValue out);
// navigation_api.cpp: the Navigation members (slot = NavigationMemberId; on*
// handler getters 50 + i, setters 150 + i) and the window.navigation accessor
// (slot 0 get, 1 set).
enum NavigationMemberId { mEntries, mUpdate, mNavigate, mReload, mTraverse, mBack, mForward, mCurrent,
                          mTransition, mActivation, mCanBack, mCanForward, kNavigationMemberCount };
extern const char* const kNavigationMemberNames[kNavigationMemberCount];
extern const char* const kNavigationHandlerTypes[4];
bool NavigationMember(JSContext* cx, unsigned argc, JS::Value* vp);
bool NavigationWindowAccessor(JSContext* cx, unsigned argc, JS::Value* vp);
// navigation_entries.cpp
bool NavigationEntries(NavigationState& n, std::vector<Model::HistoryView>* views, int32_t* index);
bool NavigationEntryObject(NavigationState& n, const Model::HistoryView& view, JS::MutableHandleObject out);
bool NavigationCurrentEntry(NavigationState& n, JS::MutableHandleValue out);  // null when none
bool NewNavigationDestination(NavigationState& n, const std::string& url, bool sameDocument,
                              JS::HandleValue state, JS::MutableHandleObject out);
bool InstallNavigationEntries(NavigationState& n);
// NavigationPrecommitController, interface object only (presentation_views.cpp).
bool InstallNavigationPrecommit(NavigationState& n);
// navigation_events.cpp. `proceed` is false when a navigate listener cancelled.
// `from` receives the current entry before the commit (null without a Navigation).
bool NavigationBeforeCommit(State& state, const char* type, const std::string& url, bool hashChange,
                            JS::HandleValue info, JS::HandleValue navigationState,
                            JS::MutableHandleValue from, bool* proceed);
bool NavigationAfterCommit(State& state, const char* type, JS::HandleValue from);
bool NavigationFireChange(NavigationState& n, const char* type, JS::HandleValue from);
}  // namespace ruyijs::browser_environment
#endif
