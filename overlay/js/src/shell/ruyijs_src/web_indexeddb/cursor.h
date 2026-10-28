#pragma once
// IDBCursor native state (cursor_open.cpp, cursor_members.cpp).
#include "web_indexeddb/keys.h"
namespace ruyijs::web_indexeddb {
struct NativeCursor {
  std::u16string index;
  bool hasIndex = false, keyOnly = false, gotValue = false, finished = false;
  KeyRange range;
  Direction direction = Direction::Next;
  CursorPosition position;
};
NativeCursor* CursorData(JSObject* cursor);
bool ScheduleStep(State&, JS::HandleObject cursor, const CursorStep&);
}  // namespace ruyijs::web_indexeddb
