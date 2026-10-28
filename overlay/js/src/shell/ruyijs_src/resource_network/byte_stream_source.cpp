// Blob.stream(): the body-holder byte stream that Response.body uses, over a copy of
// the Blob's bytes (Firefox creates Blob streams as byte streams too).
#include "resource_network/realm.h"
#include "resource_network/body.h"
#include "resource_network/readable_stream.h"
#include "resource_network/state.h"
#include "js/Realm.h"
#include <new>

namespace ruyijs::resource_network {
bool Realm::NewByteStream(std::string_view bytes, JS::MutableHandleObject stream) {
  State& state = *state_;
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.page);
  JS::RootedObject holder(cx, NewBodyHolder(state));
  if (!holder) return false;
  auto* record = new (std::nothrow) BodyRecord();
  if (!record) { JS_ReportOutOfMemory(cx); return false; }
  record->bytes.assign(bytes.data(), bytes.size());
  record->kind = BodyKind::Bytes;
  record->nullBody = false;
  CommitBodyHolder(holder, record);
  return EnsureBodyStream(cx, holder, stream);
}
}  // namespace ruyijs::resource_network
