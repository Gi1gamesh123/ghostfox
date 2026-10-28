// Index key paths: validation (WebIDL (DOMString or sequence<DOMString>) plus
// the IndexedDB valid-key-path grammar), evaluation on a record's clone, and
// the index keys a record contributes (multiEntry arrays expand, invalid keys
// contribute nothing). docs/owners/INDEXED_DB.md, 2026-10-21.
#include "web_indexeddb/keys.h"
#include "js/Array.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/PropertyDescriptor.h"
#include <algorithm>

namespace ruyijs::web_indexeddb {
namespace {
bool IdentifierStart(char16_t c) { return c == u'$' || c == u'_' || (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z') || c > 127; }
bool IdentifierPart(char16_t c) { return IdentifierStart(c) || (c >= u'0' && c <= u'9'); }
bool ValidPath(const std::u16string& path) {
  if (path.empty()) return true;
  size_t start = 0;
  while (start <= path.size()) {
    size_t end = path.find(u'.', start);
    if (end == std::u16string::npos) end = path.size();
    if (end == start || !IdentifierStart(path[start])) return false;
    for (size_t i = start + 1; i < end; ++i) if (!IdentifierPart(path[i])) return false;
    start = end + 1;
  }
  return true;
}
// One key path on a value: identifiers walk own properties, with the
// String/Array length special case. False in *found when a step is missing.
bool Evaluate(State& s, JS::HandleValue input, const std::u16string& path, JS::MutableHandleValue out, bool* found) {
  JSContext* cx = s.cx;
  *found = true;
  out.set(input);
  if (path.empty()) return true;
  size_t start = 0;
  while (start <= path.size()) {
    size_t end = path.find(u'.', start);
    if (end == std::u16string::npos) end = path.size();
    const std::u16string id = path.substr(start, end - start);
    start = end + 1;
    if (out.isString() && id == u"length") { out.setNumber(double(JS_GetStringLength(out.toString()))); continue; }
    if (!out.isObject()) { *found = false; return true; }
    JS::RootedObject object(cx, &out.toObject());
    JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> own(cx);
    if (!JS_GetOwnUCPropertyDescriptor(cx, object, id.data(), id.size(), &own)) return false;
    if (own.isNothing()) { *found = false; return true; }
    if (!JS_GetUCProperty(cx, object, id.data(), id.size(), out)) return false;
  }
  return true;
}
// EncodeKey reports an invalid key as a pending DataError; here it is "no key".
bool TryEncode(State& s, JS::HandleValue value, std::u16string* out, bool* ok) {
  *ok = EncodeKey(s, value, out);
  if (*ok) return true;
  if (!JS_IsExceptionPending(s.cx) || JS_IsThrowingOutOfMemory(s.cx)) return false;
  JS_ClearPendingException(s.cx);
  return true;
}
}  // namespace

bool ValidKeyPath(JSContext* cx, JS::HandleValue value, IndexInfo* info, bool* valid) {
  info->paths.clear();
  info->array = false;
  bool isArray = false;
  if (value.isObject()) {
    JS::RootedObject object(cx, &value.toObject());
    if (!JS::IsArrayObject(cx, object, &isArray)) return false;
    if (isArray) {
      uint32_t length = 0;
      if (!JS::GetArrayLength(cx, object, &length)) return false;
      info->array = true;
      for (uint32_t i = 0; i < length; ++i) {
        JS::RootedValue item(cx);
        std::u16string path;
        if (!JS_GetElement(cx, object, i, &item) || !Text(cx, item, &path)) return false;
        info->paths.push_back(std::move(path));
      }
      *valid = length > 0;
      for (const auto& path : info->paths) *valid = *valid && ValidPath(path);
      return true;
    }
  }
  std::u16string path;
  if (!Text(cx, value, &path)) return false;
  info->paths.push_back(path);
  *valid = ValidPath(path);
  return true;
}
bool KeyPathValue(State& s, const IndexInfo& info, JS::MutableHandleValue out) {
  if (!info.array) return String(s.cx, info.paths.empty() ? std::u16string() : info.paths[0], out);
  JS::RootedObject array(s.cx, JS::NewArrayObject(s.cx, info.paths.size()));
  if (!array) return false;
  for (uint32_t i = 0; i < info.paths.size(); ++i) {
    JS::RootedValue item(s.cx);
    if (!String(s.cx, info.paths[i], &item) || !JS_SetElement(s.cx, array, i, item)) return false;
  }
  out.setObject(*array);
  return true;
}
bool IndexKeysOf(State& s, const IndexInfo& info, JS::HandleValue clone, std::vector<std::u16string>* out) {
  JSContext* cx = s.cx;
  out->clear();
  JS::RootedValue value(cx);
  bool found = false, ok = false;
  std::u16string key;
  if (info.array) {
    JS::RootedObject array(cx, JS::NewArrayObject(cx, info.paths.size()));
    if (!array) return false;
    for (uint32_t i = 0; i < info.paths.size(); ++i) {
      if (!Evaluate(s, clone, info.paths[i], &value, &found)) return false;
      if (!found) return true;
      if (!JS_SetElement(cx, array, i, value)) return false;
    }
    value.setObject(*array);
    if (!TryEncode(s, value, &key, &ok)) return false;
    if (ok) out->push_back(std::move(key));
    return true;
  }
  if (!Evaluate(s, clone, info.paths.empty() ? std::u16string() : info.paths[0], &value, &found)) return false;
  if (!found) return true;
  bool isArray = false;
  if (info.multiEntry && value.isObject()) {
    JS::RootedObject object(cx, &value.toObject());
    if (!JS::IsArrayObject(cx, object, &isArray)) return false;
    if (isArray) {
      uint32_t length = 0;
      if (!JS::GetArrayLength(cx, object, &length)) return false;
      for (uint32_t i = 0; i < length; ++i) {
        JS::RootedValue item(cx);
        if (!JS_GetElement(cx, object, i, &item) || !TryEncode(s, item, &key, &ok)) return false;
        if (ok && std::find(out->begin(), out->end(), key) == out->end()) out->push_back(key);
      }
      return true;
    }
  }
  if (!TryEncode(s, value, &key, &ok)) return false;
  if (ok) out->push_back(std::move(key));
  return true;
}
bool RecordIndexKeys(State& s, const Key& database, const std::u16string& store, JS::HandleValue clone, IndexKeys* out) {
  std::vector<IndexInfo> indexes;
  if (!s.store->model.Indexes(database, store, &indexes)) return Throw(s, "UnknownError");
  for (const auto& info : indexes) {
    std::vector<std::u16string> keys;
    if (!IndexKeysOf(s, info, clone, &keys)) return false;
    if (!keys.empty()) (*out)[info.name] = std::move(keys);
  }
  return true;
}
}  // namespace ruyijs::web_indexeddb
