// URLSearchParams. The object owns one encoded query string; every operation
// parses it, edits the ordered name/value list and re-serializes, so duplicate
// keys keep their relative order and a bound URL is written back through the
// URL owner instead of a second copy of the query.
#include "web_platform/state.h"
#include "runtime/web_kit.h"
#include "runtime/native_constructor_trace.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/ValueArray.h"
#include <algorithm>

namespace ruyijs::web_platform {
namespace {
using Entry = std::pair<std::string, std::string>;

// Method ids carried in the function's reserved slot.
bool Check(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleObject self) {
  return WebKit::Check(cx, args.thisv(), &kSearchParamsClass, self,
                       "URLSearchParams member called on an incompatible receiver");
}

bool Store(State& state, JS::HandleObject self, std::vector<Entry>& entries) {
  return StoreEntries(state, self, entries);
}

bool Read(JSContext* cx, JS::HandleObject self, std::vector<Entry>* entries) {
  return ParamsEntries(cx, self, entries);
}
}  // namespace

bool ParamsConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  NativeConstructorTrace trace(cx, args);
  std::string query;
  if (argc >= 1 && !args.get(0).isUndefined() && !args.get(0).isNull()) {
    JS::HandleValue init = args.get(0);
    if (IsSearchParams(init)) {
      JS::RootedValue slot(cx, JS::GetReservedSlot(&init.toObject(), kParamsQuerySlot));
      if (!Convert(cx, slot, &query)) return false;
    } else if (init.isObject()) {
      std::vector<Entry> entries;
      if (!ConvertInit(cx, init, &entries)) return false;
      query = SerializeFormQuery(entries);
    } else {
      if (!Convert(cx, init, &query)) return false;
      if (!query.empty() && query[0] == '?') query.erase(0, 1);
    }
  }
  JS::RootedObject prototype(cx);
  if (!state.kit.ConstructProto(args, state.params.prototype, &prototype)) return false;
  JS::RootedObject object(cx, state.kit.New(&kSearchParamsClass, prototype));
  JS::RootedValue text(cx);
  if (!object || !Value(cx, query, &text)) return false;
  JS::SetReservedSlot(object, kParamsQuerySlot, text);
  JS::SetReservedSlot(object, kParamsOwnerSlot, JS::UndefinedValue());
  args.rval().setObject(*object);
  return trace.Finish(true);
}

bool ParamsGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Check(cx, args, &self)) return false;
  std::vector<Entry> entries;
  if (!Read(cx, self, &entries)) return false;
  args.rval().setInt32(int32_t(entries.size()));
  return true;
}

bool ParamsMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  if (!Check(cx, args, &self)) return false;
  std::vector<Entry> entries;
  if (!Read(cx, self, &entries)) return false;
  std::string name, value;
  switch (ParamsMethodKind(WebKit::Slot(args))) {
    case kAppend:
      if (!Convert(cx, args.get(0), &name) || !Convert(cx, args.get(1), &value)) return false;
      entries.emplace_back(std::move(name), std::move(value));
      args.rval().setUndefined();
      return Store(state, self, entries);
    case kDelete: {
      if (!Convert(cx, args.get(0), &name)) return false;
      entries.erase(std::remove_if(entries.begin(), entries.end(),
                                   [&name](const Entry& entry) { return entry.first == name; }),
                    entries.end());
      args.rval().setUndefined();
      return Store(state, self, entries);
    }
    case kGet: {
      if (!Convert(cx, args.get(0), &name)) return false;
      for (const Entry& entry : entries)
        if (entry.first == name) return Value(cx, entry.second, args.rval());
      args.rval().setNull();
      return true;
    }
    case kGetAll: {
      if (!Convert(cx, args.get(0), &name)) return false;
      JS::RootedObject list(cx, JS::NewArrayObject(cx, 0));
      uint32_t index = 0;
      if (!list) return false;
      for (const Entry& entry : entries) {
        JS::RootedValue item(cx);
        if (entry.first != name) continue;
        if (!Value(cx, entry.second, &item) || !JS_SetElement(cx, list, index++, item))
          return false;
      }
      args.rval().setObject(*list);
      return true;
    }
    case kHas: {
      if (!Convert(cx, args.get(0), &name)) return false;
      bool found = false;
      for (const Entry& entry : entries) found = found || entry.first == name;
      args.rval().setBoolean(found);
      return true;
    }
    case kSet: {
      if (!Convert(cx, args.get(0), &name) || !Convert(cx, args.get(1), &value)) return false;
      size_t slot = entries.size();
      std::vector<Entry> next;
      for (Entry& entry : entries) {
        if (entry.first != name) { next.push_back(std::move(entry)); continue; }
        if (slot == entries.size()) { slot = next.size(); next.emplace_back(name, value); }
      }
      if (slot == entries.size()) next.emplace_back(std::move(name), std::move(value));
      args.rval().setUndefined();
      return Store(state, self, next);
    }
    case kSort: {
      std::stable_sort(entries.begin(), entries.end(),
                       [](const Entry& left, const Entry& right) {
                         return left.first < right.first;
                       });
      args.rval().setUndefined();
      return Store(state, self, entries);
    }
    case kToString: {
      std::string text = SerializeFormQuery(entries);
      return Value(cx, text, args.rval());
    }
    case kForEach: {
      JS::RootedValue callback(cx, args.get(0));
      if (!callback.isObject() || !JS::IsCallable(&callback.toObject()))
        return WebKit::Fail(cx, "URLSearchParams forEach requires a callback");
      JS::RootedValue this_arg(cx, args.get(1));
      // WebIDL pair iteration reads the current list by index each step, so a
      // callback that deletes or appends is seen (Firefox: delete("b") while
      // visiting "a" skips "b").
      for (size_t index = 0; index < entries.size(); ++index) {
        const Entry entry = entries[index];
        JS::RootedValue item(cx), key(cx), result(cx);
        JS::RootedValueArray<3> call_args(cx);
        if (!Value(cx, entry.second, &item) || !Value(cx, entry.first, &key)) return false;
        call_args[0].set(item);
        call_args[1].set(key);
        call_args[2].setObject(*self);
        if (!JS::Call(cx, this_arg, callback, call_args, &result)) return false;
        entries.clear();  // Read appends
        if (!Read(cx, self, &entries)) return false;
      }
      args.rval().setUndefined();
      return true;
    }
  }
  return WebKit::Fail(cx, "Unknown URLSearchParams member");
}

}  // namespace ruyijs::web_platform
