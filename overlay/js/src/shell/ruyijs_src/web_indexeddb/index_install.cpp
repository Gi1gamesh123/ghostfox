// IDBIndex, IDBCursor and IDBCursorWithValue interfaces: one installer, Firefox
// member order, constructor last (docs/owners/INDEXED_DB.md, 2026-10-21).
#include "web_indexeddb/keys.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::web_indexeddb {
const JSClass kIndexClass = {"IDBIndex", JSCLASS_HAS_RESERVED_SLOTS(IndexSlots)};
namespace {
bool Last(State& s, WebFace& face) {
  return JS_DefineProperty(s.cx, face.prototype, "constructor", face.constructor, 0);
}
}  // namespace
bool InstallIndexes(State& s) {
  JS::RootedObject none(s.cx);
  if (!s.kit.Define("IDBIndex", &kIndexClass, none, WebKit::Reject, &s.indexFace) ||
      !JS_DeleteProperty(s.cx, s.indexFace.prototype, "constructor")) return false;
  struct Method { const char* name; unsigned length; int kind; };
  const Method methods[] = {{"get", 1, Get}, {"getKey", 1, GetKey}, {"getAll", 0, GetAll}, {"mozGetAll", 0, GetAll},
                            {"getAllKeys", 0, GetAllKeys}, {"mozGetAllKeys", 0, GetAllKeys},
                            {"getAllRecords", 0, GetAllRecords}, {"count", 0, Count}, {"openCursor", 0, 26},
                            {"openKeyCursor", 0, 27}};
  for (const auto& method : methods)
    if (!s.kit.Method(s.indexFace, "IDBIndex", method.name, IndexMethod, method.length, method.kind)) return false;
  const char* const fields[] = {"name", "objectStore", "keyPath", "multiEntry", "unique"};
  for (int i = 0; i < 5; ++i)
    if (!s.kit.Accessor(s.indexFace, "IDBIndex", fields[i], IndexGet, i == 0 ? IndexSetName : nullptr, i)) return false;
  if (!Last(s, s.indexFace)) return false;
  if (!s.kit.Define("IDBCursor", &kCursorClass, none, WebKit::Reject, &s.cursorFace) ||
      !JS_DeleteProperty(s.cx, s.cursorFace.prototype, "constructor")) return false;
  const char* const cursorMethods[] = {"advance", "continue", "continuePrimaryKey", "update", "delete"};
  const unsigned lengths[] = {1, 0, 2, 1, 0};
  for (int i = 0; i < 5; ++i)
    if (!s.kit.Method(s.cursorFace, "IDBCursor", cursorMethods[i], CursorMethod, lengths[i], i)) return false;
  const char* const cursorFields[] = {"source", "direction", "key", "primaryKey", "request"};
  for (int i = 0; i < 5; ++i)
    if (!s.kit.Accessor(s.cursorFace, "IDBCursor", cursorFields[i], CursorGet, nullptr, i)) return false;
  if (!Last(s, s.cursorFace)) return false;
  JS::RootedObject parent(s.cx, s.cursorFace.prototype);
  if (!s.kit.Define("IDBCursorWithValue", &kCursorValueClass, parent, WebKit::Reject, &s.cursorValueFace) ||
      !JS_SetPrototype(s.cx, s.cursorValueFace.constructor, s.cursorFace.constructor) ||
      !JS_DeleteProperty(s.cx, s.cursorValueFace.prototype, "constructor") ||
      !s.kit.Accessor(s.cursorValueFace, "IDBCursorWithValue", "value", CursorGet, nullptr, 5)) return false;
  return Last(s, s.cursorValueFace);
}
}  // namespace ruyijs::web_indexeddb
