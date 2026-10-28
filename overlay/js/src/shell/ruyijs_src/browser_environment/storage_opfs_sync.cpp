// FileSystemFileHandle.createSyncAccessHandle() and FileSystemSyncAccessHandle
// (dedicated Workers only). The handle reads and writes the node's bytes in place
// through a file position cursor and holds the node's exclusive lock until close().
#include <thread>
#include <chrono>
#include "browser_environment/storage_opfs.h"
#include "browser_environment/state.h"
#include "browser_environment/window_members.h"
#include "runtime/runtime.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
#include "js/experimental/TypedData.h"
#include <cmath>
#include <cstring>
#include <new>

namespace ruyijs::browser_environment {
namespace {
// Slots: 0 node holder, 1 cursor (double), 2 closed (bool).
enum Member { kRead, kWrite, kTruncate, kGetSize, kFlush, kClose };
const char* const kNames[] = {"read", "write", "truncate", "getSize", "flush", "close"};
void FinalizeSync(JS::GCContext*, JSObject* object) {
  const JS::Value slot = JS::GetReservedSlot(object, 0);
  if (slot.isUndefined()) return;
  auto* holder = static_cast<std::shared_ptr<OpfsNode>*>(slot.toPrivate());
  if (!JS::GetReservedSlot(object, 2).toBoolean()) (*holder)->exclusiveLock = false;
  delete holder;
}
const JSClassOps kSyncOps = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, FinalizeSync};
const JSClass kSyncClass = {"FileSystemSyncAccessHandle",
    JSCLASS_HAS_RESERVED_SLOTS(3) | JSCLASS_FOREGROUND_FINALIZE, &kSyncOps};
State& Of(const JS::CallArgs& args) {
  return *static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
}
bool Fail(JSContext* cx, int member, const char* detail) {
  std::string text = std::string("FileSystemSyncAccessHandle.") + kNames[member] + ": " + detail;
  return Error(cx, text.c_str());
}
// [EnforceRange] unsigned long long.
bool Range(JSContext* cx, JS::HandleValue value, int member, const char* what, double* out) {
  if (!JS::ToNumber(cx, value, out)) return false;
  if (std::isfinite(*out)) *out = std::trunc(*out);
  if (!std::isfinite(*out) || *out < 0 || *out > 9007199254740991.0)
    return Fail(cx, member, (std::string(what) + " is out of range for unsigned long long.").c_str());
  return true;
}
bool Member(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Of(args);
  const int member = WebKit::Slot(args);
  JSObject* raw = args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr;
  if (!raw || JS::GetClass(raw) != &kSyncClass) return WebKit::FailBrand(cx, args, "FileSystemSyncAccessHandle");
  JS::RootedObject self(cx, raw);
  OpfsNode& node = **static_cast<std::shared_ptr<OpfsNode>*>(JS::GetReservedSlot(raw, 0).toPrivate());
  s.Observe("FileSystemSyncAccessHandle", kNames[member], 0);
  const bool closed = JS::GetReservedSlot(raw, 2).toBoolean();
  if (member == kClose) {
    if (!closed) node.exclusiveLock = false;
    JS::SetReservedSlot(self, 2, JS::TrueValue());
    args.rval().setUndefined();
    return true;
  }
  if (member == kRead || member == kWrite) {
    JSObject* source = args.get(0).isObject() ? js::CheckedUnwrapStatic(&args.get(0).toObject()) : nullptr;
    const bool view = source && JS_IsArrayBufferViewObject(source);
    if (!source || (!view && !JS::IsArrayBufferObject(source)))
      return Fail(cx, member, "Argument 1 could not be converted to any of: ArrayBuffer, ArrayBufferView.");
    JS::RootedObject buffer(cx, source);
    double at = JS::GetReservedSlot(raw, 1).toDouble();
    if (args.get(1).isObject()) {
      JS::RootedObject options(cx, &args.get(1).toObject());
      JS::RootedValue value(cx);
      if (!JS_GetProperty(cx, options, "at", &value)) return false;
      if (!value.isUndefined() &&
          !Range(cx, value, member, "'at' member of FileSystemReadWriteOptions", &at)) return false;
    } else if (!args.get(1).isNullOrUndefined()) {
      return Fail(cx, member, "Argument 2 can't be converted to a dictionary.");
    }
    if (closed) return ThrowDOM(s, "InvalidStateError",
                                (std::string("FileSystemSyncAccessHandle.") + kNames[member] +
                                 ": SyncAccessHandle is closed").c_str());
    JS::AutoCheckCannotGC noGC(cx);
    bool shared = false;
    const size_t length = view ? JS_GetArrayBufferViewByteLength(buffer) : JS::GetArrayBufferByteLength(buffer);
    uint8_t* bytes = view ? static_cast<uint8_t*>(JS_GetArrayBufferViewData(buffer, &shared, noGC))
                          : JS::GetArrayBufferData(buffer, &shared, noGC);
    size_t position = size_t(at), done = 0;
    if (member == kRead) {
      if (position < node.data.size()) {
        done = std::min(length, node.data.size() - position);
        std::memcpy(bytes, node.data.data() + position, done);
      }
    } else {
      if (position + length > node.data.size()) node.data.resize(position + length, '\0');
      if (length) std::memcpy(&node.data[position], bytes, length);
      done = length;
    }
    JS::SetReservedSlot(self, 1, JS::DoubleValue(double(position + done)));
    args.rval().setNumber(double(done));
    return true;
  }
  double size = 0;
  if (member == kTruncate && !Range(cx, args.get(0), member, "Argument 1", &size)) return false;
  if (closed) return ThrowDOM(s, "InvalidStateError",
                              (std::string("FileSystemSyncAccessHandle.") + kNames[member] +
                               ": SyncAccessHandle is closed").c_str());
  if (member == kGetSize) { args.rval().setNumber(double(node.data.size())); return true; }
  if (member == kTruncate) {
    node.data.resize(size_t(size), '\0');
    if (JS::GetReservedSlot(raw, 1).toDouble() > size) JS::SetReservedSlot(self, 1, JS::DoubleValue(size));
  }
  if (member == kFlush) {
    // A real FileSystemSyncAccessHandle.flush() blocks on the filesystem's
    // durable write; Firefox's takes about a millisecond. Cloudflare Turnstile
    // times flush() as an anti-emulation signal (worker iLkS0 = perf.now()
    // delta across flush), so an in-memory no-op that returns instantly reads
    // as fake. Busy-spin ~1ms of real monotonic time (sleep_for overshoots on
    // Windows' ~15ms timer granularity) so the measured delta matches a genuine
    // flush without over-reporting.
    const auto spinUntil = std::chrono::steady_clock::now() + std::chrono::microseconds(1050);
    while (std::chrono::steady_clock::now() < spinUntil) { /* spin */ }
  }
  args.rval().setUndefined();
  return true;
}
}  // namespace

bool OpfsCreateSyncAccessHandle(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Of(args);
  auto* node = OpfsNodeOf(args.thisv(), &kOpfsFileClass);
  if (!node) { WebKit::FailBrand(cx, args, "FileSystemFileHandle"); return OpfsRejectPending(cx, args.rval()); }
  s.Observe("FileSystemFileHandle", "createSyncAccessHandle", 0);
  if (!OpfsTree::Live(**node)) return OpfsRejectStatus(s, OpfsStatus::NotFound, args.rval());
  if ((*node)->exclusiveLock)
    return OpfsReject(s, "NoModificationAllowedError", "No modification allowed", args.rval());
  JS::RootedObject prototype(cx, s.opfsStore->prototype(kOpfsSync));
  JS::RootedObject handle(cx, JS_NewObjectWithGivenProto(cx, &kSyncClass, prototype));
  auto* holder = handle ? new (std::nothrow) std::shared_ptr<OpfsNode>(*node) : nullptr;
  if (!holder) { if (handle) JS_ReportOutOfMemory(cx); return false; }
  JS::SetReservedSlot(handle, 0, JS::PrivateValue(holder));
  JS::SetReservedSlot(handle, 1, JS::DoubleValue(0));
  JS::SetReservedSlot(handle, 2, JS::FalseValue());
  (*node)->exclusiveLock = true;
  JS::RootedValue result(cx, JS::ObjectValue(*handle));
  return OpfsResolve(cx, result, args.rval());
}

bool InstallOpfsSync(State& s, JS::HandleObject prototype) {
  const unsigned lengths[] = {1, 1, 1, 0, 0, 0};
  for (int member = kRead; member <= kClose; ++member) {
    JS::RootedObject fn(s.cx, s.Function(Member, kNames[member], lengths[member], member));
    if (!fn || !JS_DefineProperty(s.cx, prototype, kNames[member], fn, JSPROP_ENUMERATE)) return false;
  }
  return true;
}
}  // namespace ruyijs::browser_environment
