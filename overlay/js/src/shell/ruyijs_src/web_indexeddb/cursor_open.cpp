// IDBCursor creation and iteration jobs (docs/owners/INDEXED_DB.md, 2026-10-21).
// A cursor's native state is its source, range, direction and position; each
// step reads the transaction's visible rows from the one Catalog.
#include "web_indexeddb/cursor.h"
#include "js/Object.h"

namespace ruyijs::web_indexeddb {
namespace {
void Finalize(JS::GCContext*, JSObject* object) {
  const JS::Value& slot = JS::GetReservedSlot(object, CursorNative);
  if (!slot.isUndefined()) delete static_cast<NativeCursor*>(slot.toPrivate());
}
const JSClassOps kOps = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, Finalize};
}  // namespace
const JSClass kCursorClass = {"IDBCursor", JSCLASS_HAS_RESERVED_SLOTS(CursorSlots) | JSCLASS_FOREGROUND_FINALIZE, &kOps};
const JSClass kCursorValueClass = {"IDBCursorWithValue",
                                   JSCLASS_HAS_RESERVED_SLOTS(CursorSlots) | JSCLASS_FOREGROUND_FINALIZE, &kOps};
NativeCursor* CursorData(JSObject* cursor) {
  return static_cast<NativeCursor*>(JS::GetReservedSlot(cursor, CursorNative).toPrivate());
}
bool ParseDirection(JSContext* cx, JS::HandleValue value, const char* method, Direction* out, std::u16string* text) {
  *text = u"next";
  if (!value.isUndefined() && !Text(cx, value, text)) return false;
  if (*text == u"next") *out = Direction::Next;
  else if (*text == u"nextunique") *out = Direction::NextUnique;
  else if (*text == u"prev") *out = Direction::Prev;
  else if (*text == u"prevunique") *out = Direction::PrevUnique;
  else {
    std::string narrow(text->begin(), text->end());
    return WebKit::Fail(cx, (std::string(method) + ": '" + narrow +
                             "' (value of argument 2) is not a valid value for enumeration IDBCursorDirection.").c_str());
  }
  return true;
}
bool ScheduleStep(State& s, JS::HandleObject cursor, const CursorStep& step) {
  JSContext* cx = s.cx;
  JS::RootedObject request(cx, &JS::GetReservedSlot(cursor, CursorRequest).toObject());
  JS::RootedObject tx(cx, &JS::GetReservedSlot(request, TransactionSlot).toObject());
  JS::SetReservedSlot(request, ReadySlot, JS::BooleanValue(false));
  CursorData(cursor)->gotValue = false;
  auto job = std::make_unique<Job>(&s, cx, request, Key{}, 0, false);
  job->kind = CursorStepKind;
  job->cursor = cursor;
  job->step = step;
  return EnqueueJob(s, std::move(job), tx);
}
bool OpenCursor(State& s, JS::HandleObject source, JS::HandleObject tx, const std::u16string* index,
                JS::HandleValue rangeValue, JS::HandleValue directionValue, bool keyOnly, JS::MutableHandleValue result) {
  JSContext* cx = s.cx;
  const char* method = index ? (keyOnly ? "IDBIndex.openKeyCursor" : "IDBIndex.openCursor")
                             : (keyOnly ? "IDBObjectStore.openKeyCursor" : "IDBObjectStore.openCursor");
  Direction direction;
  std::u16string directionText;
  if (!ParseDirection(cx, directionValue, method, &direction, &directionText)) return false;
  if (JS::GetReservedSlot(tx, TxActive).toInt32()) return Throw(s, "TransactionInactiveError");
  auto native = std::make_unique<NativeCursor>();
  if (!ReadRange(s, rangeValue, &native->range, true)) return false;
  native->direction = direction;
  native->keyOnly = keyOnly;
  native->hasIndex = index != nullptr;
  if (index) native->index = *index;
  {
    JSAutoRealm realm(cx, source);
    JS::RootedObject request(cx, NewRequest(s, source, tx));
    JS::RootedObject cursor(cx, keyOnly ? s.kit.New(&kCursorClass, s.cursorFace.prototype)
                                        : s.kit.New(&kCursorValueClass, s.cursorValueFace.prototype));
    JS::RootedValue directionString(cx);
    if (!request || !cursor || !String(cx, directionText, &directionString)) return false;
    JS::SetReservedSlot(cursor, CursorOwner, JS::PrivateValue(&s));
    JS::SetReservedSlot(cursor, CursorNative, JS::PrivateValue(native.release()));
    JS::SetReservedSlot(cursor, CursorSource, JS::ObjectValue(*source));
    JS::SetReservedSlot(cursor, CursorRequest, JS::ObjectValue(*request));
    JS::SetReservedSlot(cursor, CursorDirection, directionString);
    for (uint32_t slot : {CursorKey, CursorPrimary, CursorValue}) JS::SetReservedSlot(cursor, slot, JS::UndefinedValue());
    if (!ScheduleStep(s, cursor, CursorStep{})) return false;
    result.setObject(*request);
  }
  return JS_WrapValue(cx, result);
}
// One iteration step: the next visible row after the cursor's position.
bool CursorJob(Job& job, const char** error, JS::MutableHandleValue result) {
  State& s = *job.owner;
  JSContext* cx = s.cx;
  JS::RootedObject cursor(cx, job.cursor);
  NativeCursor& native = *CursorData(cursor);
  JS::RootedObject tx(cx, &JS::GetReservedSlot(job.request, TransactionSlot).toObject());
  std::vector<CursorRow> rows;
  if (!s.store->model.Rows(job.txId, native.hasIndex ? &native.index : nullptr,
                           native.range, native.direction, &rows)) {
    *error = "UnknownError";
    return true;
  }
  const auto next = domain::indexed_database::CursorAdvance(rows, native.position, native.direction, job.step);
  if (!next) {
    native.position.started = true;
    native.finished = true;
    for (uint32_t slot : {CursorKey, CursorPrimary, CursorValue}) JS::SetReservedSlot(cursor, slot, JS::UndefinedValue());
    result.setNull();
    return true;
  }
  native.position = {true, next->key, next->primary};
  JS::RootedValue key(cx), primary(cx), value(cx);
  if (!DecodeKey(s, next->key, &key) || !DecodeKey(s, next->primary, &primary)) return false;
  if (!native.keyOnly && !CloneRead(s, next->value, &value)) return false;
  JS::SetReservedSlot(cursor, CursorKey, key);
  JS::SetReservedSlot(cursor, CursorPrimary, primary);
  JS::SetReservedSlot(cursor, CursorValue, value);
  native.gotValue = true;
  result.setObject(*cursor);
  return true;
}
}  // namespace ruyijs::web_indexeddb
