// The FormData entry operations. append pushes one pair; set replaces the first
// match and drops the rest; delete rebuilds the list without the matches; get/
// getAll/has read it; forEach walks it in order. Every mutation writes the rebuilt
// array back to the one slot, so nothing caches a stale copy. The shared ordered-
// list helpers live in form_data_internal.h.
#include "web_platform/state.h"
#include "web_platform/form_data_internal.h"

#include "runtime/web_kit.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include <string>

namespace ruyijs::web_platform {
namespace {
using form_data_internal::Entry;
using form_data_internal::Length;
using form_data_internal::Match;
using form_data_internal::Name;
using form_data_internal::NewPair;
using form_data_internal::Pair;
using form_data_internal::Push;

bool Append(State& state, JS::HandleObject list, JS::CallArgs& args) {
  JSContext* cx = state.cx;
  JS::RootedValue name(cx);
  std::string key;
  if (args.length() < 2) return WebKit::Fail(cx, "FormData.append requires a name and a value");
  if (!Name(cx, args, &name, &key)) return false;
  JS::RootedValue value(cx);
  if (!FormDataValue(state, args[1], args.get(2), &value)) return false;
  JS::RootedObject pair(cx);
  uint32_t length = 0;
  if (!NewPair(cx, name, value, &pair) || !Length(cx, list, &length) ||
      !Push(cx, list, length, pair))
    return false;
  args.rval().setUndefined();
  return true;
}

// set: emit the new pair where the first match stood, skip the other matches, and
// append when nothing matched.
bool Set(State& state, JS::HandleObject self, JS::HandleObject list, JS::CallArgs& args) {
  JSContext* cx = state.cx;
  JS::RootedValue name(cx);
  std::string key;
  if (args.length() < 2) return WebKit::Fail(cx, "FormData.set requires a name and a value");
  if (!Name(cx, args, &name, &key)) return false;
  JS::RootedValue value(cx);
  if (!FormDataValue(state, args[1], args.get(2), &value)) return false;
  JS::RootedObject pair(cx);
  uint32_t length = 0;
  if (!NewPair(cx, name, value, &pair) || !Length(cx, list, &length)) return false;
  JS::RootedObject rebuilt(cx, JS::NewArrayObject(cx, 0));
  if (!rebuilt) return false;
  bool placed = false;
  uint32_t next = 0;
  for (uint32_t index = 0; index < length; ++index) {
    JS::RootedObject entry(cx);
    bool match = false;
    if (!Entry(cx, list, index, &entry) || !Match(cx, entry, key, &match)) return false;
    if (match) {
      if (placed) continue;
      placed = true;
      if (!Push(cx, rebuilt, next++, pair)) return false;
      continue;
    }
    if (!Push(cx, rebuilt, next++, entry)) return false;
  }
  if (!placed && !Push(cx, rebuilt, next, pair)) return false;
  JS::SetReservedSlot(self, kFormDataEntriesSlot, JS::ObjectValue(*rebuilt));
  args.rval().setUndefined();
  return true;
}

bool Delete(State& state, JS::HandleObject self, JS::HandleObject list, JS::CallArgs& args) {
  JSContext* cx = state.cx;
  JS::RootedValue name(cx);
  std::string key;
  if (!Name(cx, args, &name, &key)) return false;
  uint32_t length = 0;
  if (!Length(cx, list, &length)) return false;
  JS::RootedObject rebuilt(cx, JS::NewArrayObject(cx, 0));
  if (!rebuilt) return false;
  uint32_t next = 0;
  for (uint32_t index = 0; index < length; ++index) {
    JS::RootedObject entry(cx);
    bool match = false;
    if (!Entry(cx, list, index, &entry) || !Match(cx, entry, key, &match)) return false;
    if (!match && !Push(cx, rebuilt, next++, entry)) return false;
  }
  JS::SetReservedSlot(self, kFormDataEntriesSlot, JS::ObjectValue(*rebuilt));
  args.rval().setUndefined();
  return true;
}

bool Get(JSContext* cx, JS::HandleObject list, JS::CallArgs& args, bool all) {
  JS::RootedValue name(cx);
  std::string key;
  if (!Name(cx, args, &name, &key)) return false;
  uint32_t length = 0;
  if (!Length(cx, list, &length)) return false;
  JS::RootedObject matches(cx, all ? JS::NewArrayObject(cx, 0) : nullptr);
  if (all && !matches) return false;
  uint32_t next = 0;
  for (uint32_t index = 0; index < length; ++index) {
    JS::RootedObject entry(cx);
    JS::RootedValue entry_name(cx), entry_value(cx);
    bool match = false;
    if (!Entry(cx, list, index, &entry) || !Pair(cx, entry, &entry_name, &entry_value) ||
        !Match(cx, entry, key, &match))
      return false;
    if (!match) continue;
    if (!all) {
      args.rval().set(entry_value);
      return true;
    }
    JS::RootedValue item(cx, entry_value);
    if (!JS_SetElement(cx, matches, next++, item)) return false;
  }
  if (all) args.rval().setObject(*matches);
  else args.rval().setNull();
  return true;
}

bool Has(JSContext* cx, JS::HandleObject list, JS::CallArgs& args) {
  JS::RootedValue name(cx);
  std::string key;
  if (!Name(cx, args, &name, &key)) return false;
  uint32_t length = 0;
  if (!Length(cx, list, &length)) return false;
  for (uint32_t index = 0; index < length; ++index) {
    JS::RootedObject entry(cx);
    bool match = false;
    if (!Entry(cx, list, index, &entry) || !Match(cx, entry, key, &match)) return false;
    if (match) {
      args.rval().setBoolean(true);
      return true;
    }
  }
  args.rval().setBoolean(false);
  return true;
}

bool ForEach(State& state, JS::HandleObject self, JS::HandleObject list,
             JS::CallArgs& args) {
  JSContext* cx = state.cx;
  if (!args.length() || !args[0].isObject() || !JS::IsCallable(&args[0].toObject()))
    return WebKit::Fail(cx, "FormData forEach callback must be callable");
  uint32_t length = 0;
  if (!Length(cx, list, &length)) return false;
  JS::RootedValue callback(cx, args[0]), this_value(cx, args.get(1)), ignored(cx);
  JS::RootedValueArray<3> call_args(cx);
  call_args[2].set(JS::ObjectValue(*self));
  for (uint32_t index = 0; index < length; ++index) {
    JS::RootedObject entry(cx);
    JS::RootedValue name(cx), value(cx);
    if (!Entry(cx, list, index, &entry) || !Pair(cx, entry, &name, &value)) return false;
    call_args[0].set(value);
    call_args[1].set(name);
    if (!JS::Call(cx, this_value, callback, call_args, &ignored)) return false;
  }
  args.rval().setUndefined();
  return true;
}
}  // namespace

bool FormDataMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx), list(cx);
  if (!FormDataBrand(cx, args.thisv(), &self) || !FormDataList(cx, self, &list)) return false;
  switch (FormDataMethodKind(WebKit::Slot(args))) {
    case kFormDataAppend:
      return Append(state, list, args);
    case kFormDataSet:
      return Set(state, self, list, args);
    case kFormDataDelete:
      return Delete(state, self, list, args);
    case kFormDataGet:
      return Get(cx, list, args, false);
    case kFormDataGetAll:
      return Get(cx, list, args, true);
    case kFormDataHas:
      return Has(cx, list, args);
    default:
      return ForEach(state, self, list, args);
  }
}
}  // namespace ruyijs::web_platform
