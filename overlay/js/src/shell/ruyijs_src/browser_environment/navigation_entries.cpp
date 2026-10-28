// NavigationHistoryEntry, NavigationActivation, NavigationDestination and
// NavigationTransition. An entry object stands for one session-history entry
// revision: replace keeps the entry's key and gives it a new id and object.
#include "browser_environment/navigation_api.h"
#include "domain/browser_environment/url_value.h"
#include "interaction/realm.h"
#include "js/MapAndSet.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include <cstdio>
#include <random>

namespace ruyijs::browser_environment {
const JSClass kNavigationEntryClass = {"NavigationHistoryEntry", JSCLASS_HAS_RESERVED_SLOTS(kNavEntrySlots)};
const JSClass kNavigationActivationClass = {"NavigationActivation", JSCLASS_HAS_RESERVED_SLOTS(3)};
const JSClass kNavigationTransitionClass = {"NavigationTransition", JSCLASS_HAS_RESERVED_SLOTS(4)};
const JSClass kNavigationDestinationClass = {"NavigationDestination", JSCLASS_HAS_RESERVED_SLOTS(kNavDestSlots)};
namespace {
using UrlValue = ruyijs::domain::browser_environment::UrlValue;
std::string Uuid() {
  static std::mt19937_64 random{std::random_device{}()};
  const uint64_t a = random(), b = random();
  char text[40];
  std::snprintf(text, sizeof text, "%08x-%04x-4%03x-%04x-%012llx", unsigned(a >> 32), unsigned(a >> 16) & 0xffff,
                unsigned(a) & 0xfff, unsigned(0x8000 | (b >> 48 & 0x3fff)), (unsigned long long)(b & 0xffffffffffffULL));
  return text;
}
bool Str(JSContext* cx, const std::string& text, JS::MutableHandleValue out) {
  JSString* s = JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(text.data(), text.size()));
  if (!s) return false;
  out.setString(s);
  return true;
}
bool Fail(JSContext* cx, const char* member, const char* iface) {
  const std::string text = std::string("'") + member + "' called on an object that does not implement interface " + iface + ".";
  return WebKit::Fail(cx, text.c_str());
}
enum EntryMember { eGetState, eUrl, eKey, eId, eIndex, eSame, eOnDispose };
const char* const kEntryNames[] = {"getState", "url", "key", "id", "index", "sameDocument", "ondispose"};
bool EntryMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NavigationState& n = *static_cast<NavigationState*>(WebKit::Owner(args));
  const int slot = WebKit::Slot(args), member = slot % 100;
  const bool destination = slot >= 200;
  const JSClass* clasp = destination ? &kNavigationDestinationClass : &kNavigationEntryClass;
  JSObject* self = args.thisv().isObject() ? &args.thisv().toObject() : nullptr;
  if (!self || JS::GetClass(self) != clasp) {
    const std::string name = member == eGetState ? kEntryNames[member]
        : std::string(slot % 200 >= 100 ? "set " : "get ") + kEntryNames[member];
    return Fail(cx, name.c_str(), destination ? "NavigationDestination" : "NavigationHistoryEntry");
  }
  JS::MutableHandleValue rval = args.rval();
  if (destination) {
    const uint32_t map[] = {kNavDestState, kNavDestUrl, kNavDestKey, kNavDestUuid, kNavDestIndex, kNavDestSame};
    rval.set(JS::GetReservedSlot(self, map[member]));
    return true;
  }
  if (member == eOnDispose) {
    if (slot >= 100) { JS::SetReservedSlot(self, kNavEntryOnDispose, args.get(0).isObject() ? args[0] : JS::NullValue()); rval.setUndefined(); }
    else rval.set(JS::GetReservedSlot(self, kNavEntryOnDispose));
    return true;
  }
  const uint32_t direct[] = {0, kNavEntryUrl, kNavEntryKey, kNavEntryUuid, 0, kNavEntrySame};
  if (member != eGetState && member != eIndex) { rval.set(JS::GetReservedSlot(self, direct[member])); return true; }
  std::vector<Model::HistoryView> views;
  int32_t current = 0, index = -1;
  if (!NavigationEntries(n, &views, &current)) return false;
  const double id = JS::GetReservedSlot(self, kNavEntryId).toNumber();
  const double revision = JS::GetReservedSlot(self, kNavEntryRevision).toNumber();
  for (size_t i = 0; i < views.size(); ++i)
    if (double(views[i].id) == id && double(views[i].revision) == revision) index = int32_t(i);
  if (member == eIndex) { rval.setInt32(index); return true; }
  // getState(): this entry's navigation API state (undefined state reads as null here, as in Firefox).
  std::vector<Model::HistoryView> all;
  int32_t ignored = 0;
  if (!n.env.model.HistoryEntries(&all, &ignored)) { JS_ReportOutOfMemory(cx); return false; }
  std::vector<uint8_t> bytes;
  for (size_t i = 0; i < all.size(); ++i)
    if (double(all[i].id) == id && double(all[i].revision) == revision &&
        !n.env.model.HistoryNavigationState(int32_t(i), &bytes)) { JS_ReportOutOfMemory(cx); return false; }
  return DecodeState(cx, bytes, rval);
}
bool ActivationMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int slot = WebKit::Slot(args);
  const char* const names[] = {"get from", "get entry", "get navigationType"};
  JSObject* self = args.thisv().isObject() ? &args.thisv().toObject() : nullptr;
  if (!self || JS::GetClass(self) != &kNavigationActivationClass) return Fail(cx, names[slot], "NavigationActivation");
  args.rval().set(JS::GetReservedSlot(self, uint32_t(slot)));
  return true;
}
bool TransitionMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int slot = WebKit::Slot(args);
  const char* const names[] = {"get navigationType", "get from", "get committed", "get finished"};
  JSObject* self = args.thisv().isObject() ? &args.thisv().toObject() : nullptr;
  if (!self || JS::GetClass(self) != &kNavigationTransitionClass) return Fail(cx, names[slot], "NavigationTransition");
  args.rval().set(JS::GetReservedSlot(self, uint32_t(slot)));  // no instance: transition is always null here
  return true;
}
}  // namespace

// The entries the Navigation API exposes: the contiguous same-origin run around
// the current entry of a top-level document; none for a child about:blank.
bool NavigationEntries(NavigationState& n, std::vector<Model::HistoryView>* views, int32_t* index) {
  views->clear();
  *index = -1;
  if (n.env.shared) return true;
  std::vector<Model::HistoryView> all;
  int32_t current = 0;
  if (!n.env.model.HistoryEntries(&all, &current)) { JS_ReportOutOfMemory(n.env.cx); return false; }
  if (all.empty()) return true;
  const std::string origin = UrlValue(all[size_t(current)].url).origin();
  size_t first = size_t(current), last = size_t(current);
  while (first > 0 && UrlValue(all[first - 1].url).origin() == origin) --first;
  while (last + 1 < all.size() && UrlValue(all[last + 1].url).origin() == origin) ++last;
  views->assign(all.begin() + first, all.begin() + last + 1);
  *index = current - int32_t(first);
  return true;
}

bool NavigationEntryObject(NavigationState& n, const Model::HistoryView& view, JS::MutableHandleObject out) {
  JSContext* cx = n.env.cx;
  JS::RootedObject cache(cx, n.entries);
  JS::RootedValue key(cx), found(cx);
  // The URL is part of the key: the initial entry's URL follows the document
  // load (sync_document_url) without a new revision.
  if (!Str(cx, std::to_string(view.id) + ":" + std::to_string(view.revision) + ":" + view.url, &key) ||
      !JS::MapGet(cx, cache, key, &found)) return false;
  if (found.isObject()) { out.set(&found.toObject()); return true; }
  JS::RootedObject proto(cx, n.entry.prototype);
  out.set(n.kit.New(&kNavigationEntryClass, proto));
  if (!out || !n.events.Attach(out)) return false;
  std::string& entryKey = n.keys[view.id];
  if (entryKey.empty()) entryKey = Uuid();
  std::string& entryId = n.ids[{view.id, view.revision}];
  if (entryId.empty()) entryId = Uuid();
  JS::RootedValue v(cx);
  JS::SetReservedSlot(out, kNavEntryId, JS::DoubleValue(double(view.id)));
  JS::SetReservedSlot(out, kNavEntryRevision, JS::DoubleValue(double(view.revision)));
  if (!Str(cx, view.url, &v)) return false;
  JS::SetReservedSlot(out, kNavEntryUrl, v);
  if (!Str(cx, entryKey, &v)) return false;
  JS::SetReservedSlot(out, kNavEntryKey, v);
  if (!Str(cx, entryId, &v)) return false;
  JS::SetReservedSlot(out, kNavEntryUuid, v);
  JS::SetReservedSlot(out, kNavEntrySame, JS::BooleanValue(true));  // entries of this document's run
  JS::SetReservedSlot(out, kNavEntryOnDispose, JS::NullValue());
  v.setObject(*out);
  return JS::MapSet(cx, cache, key, v);
}

bool NavigationCurrentEntry(NavigationState& n, JS::MutableHandleValue out) {
  std::vector<Model::HistoryView> views;
  int32_t index = -1;
  JS::RootedObject entry(n.env.cx);
  if (!NavigationEntries(n, &views, &index)) return false;
  if (index < 0) { out.setNull(); return true; }
  if (!NavigationEntryObject(n, views[size_t(index)], &entry)) return false;
  out.setObject(*entry);
  return true;
}

bool NewNavigationDestination(NavigationState& n, const std::string& url, bool sameDocument,
                              JS::HandleValue state, JS::MutableHandleObject out) {
  JSContext* cx = n.env.cx;
  JS::RootedObject proto(cx, n.destination.prototype);
  JS::RootedValue v(cx);
  out.set(n.kit.New(&kNavigationDestinationClass, proto));
  if (!out || !Str(cx, url, &v)) return false;
  JS::SetReservedSlot(out, kNavDestUrl, v);
  JS::SetReservedSlot(out, kNavDestKey, JS::StringValue(JS_GetEmptyString(cx)));
  JS::SetReservedSlot(out, kNavDestUuid, JS::StringValue(JS_GetEmptyString(cx)));
  JS::SetReservedSlot(out, kNavDestIndex, JS::Int32Value(-1));
  JS::SetReservedSlot(out, kNavDestSame, JS::BooleanValue(sameDocument));
  JS::SetReservedSlot(out, kNavDestState, state);
  return true;
}

bool InstallNavigationEntries(NavigationState& n) {
  JSContext* cx = n.env.cx;
  JS::RootedObject target(cx, n.events.prototype()), object(cx, JS::GetRealmObjectPrototype(cx));
  if (!n.kit.Define("NavigationHistoryEntry", &kNavigationEntryClass, target, WebKit::Reject, &n.entry) ||
      !n.kit.Method(n.entry, "NavigationHistoryEntry", "getState", EntryMember, 0, eGetState)) return false;
  for (int m = eUrl; m <= eSame; ++m)
    if (!n.kit.Accessor(n.entry, "NavigationHistoryEntry", kEntryNames[m], EntryMember, nullptr, m)) return false;
  JS::RootedObject proto(cx, n.entry.prototype);
  JS::RootedObject get(cx, n.kit.Function(EntryMember, "get ondispose", 0, eOnDispose));
  JS::RootedObject set(cx, n.kit.Function(EntryMember, "set ondispose", 1, 100 + eOnDispose));
  if (!get || !set || !JS_DefineProperty(cx, proto, "ondispose", get, set, JSPROP_ENUMERATE)) return false;
  const char* const activation[] = {"from", "entry", "navigationType"};
  const char* const transition[] = {"navigationType", "from", "committed", "finished"};
  if (!n.kit.Define("NavigationActivation", &kNavigationActivationClass, object, WebKit::Reject, &n.activation) ||
      !n.kit.Define("NavigationTransition", &kNavigationTransitionClass, object, WebKit::Reject, &n.transition))
    return false;
  for (int i = 0; i < 3; ++i)
    if (!n.kit.Accessor(n.activation, "NavigationActivation", activation[i], ActivationMember, nullptr, i)) return false;
  for (int i = 0; i < 4; ++i)
    if (!n.kit.Accessor(n.transition, "NavigationTransition", transition[i], TransitionMember, nullptr, i)) return false;
  if (!n.kit.Define("NavigationDestination", &kNavigationDestinationClass, object, WebKit::Reject, &n.destination) ||
      !n.kit.Method(n.destination, "NavigationDestination", "getState", EntryMember, 0, 200 + eGetState)) return false;
  for (int m = eUrl; m <= eSame; ++m)
    if (!n.kit.Accessor(n.destination, "NavigationDestination", kEntryNames[m], EntryMember, nullptr, 200 + m))
      return false;
  return true;
}
}  // namespace ruyijs::browser_environment
