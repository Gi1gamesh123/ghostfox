// FileSystemFileHandle.createWritable() and FileSystemWritableFileStream. The writable is the Resource Network WritableStream (its global constructor) over a
// native sink, re-prototyped to FileSystemWritableFileStream. The sink edits a copy
// of the file (empty unless keepExistingData) and commits it on close, as Firefox does.
#include "browser_environment/storage_opfs.h"
#include "browser_environment/state.h"
#include "runtime/runtime.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/ArrayBuffer.h"
#include "js/Promise.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
#include "js/ValueArray.h"
#include "js/experimental/TypedData.h"
#include <chrono>
#include <cstring>
#include <new>

namespace ruyijs::browser_environment {
namespace {
struct Writer {
  std::shared_ptr<OpfsNode> node;
  std::string data;
  size_t position = 0;
};
void FinalizeWriter(JS::GCContext*, JSObject* object) {
  const JS::Value slot = JS::GetReservedSlot(object, 0);
  if (!slot.isUndefined()) delete static_cast<Writer*>(slot.toPrivate());
}
const JSClassOps kWriterOps = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, FinalizeWriter};
const JSClass kWriterClass = {"FileSystemWritableFileStream sink",
    JSCLASS_HAS_RESERVED_SLOTS(1) | JSCLASS_FOREGROUND_FINALIZE, &kWriterOps};
State& Of(const JS::CallArgs& args) {
  return *static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
}
Writer* WriterOf(const JS::CallArgs& args) {
  const JS::Value holder = js::GetFunctionNativeReserved(&args.callee(), 1);
  return holder.isObject() ? static_cast<Writer*>(JS::GetReservedSlot(&holder.toObject(), 0).toPrivate()) : nullptr;
}
double Now() {
  using namespace std::chrono;
  return double(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}
// Copies a BufferSource's bytes, or UTF-8 encodes a string; false (no exception) for other values.
bool Bytes(JSContext* cx, JS::HandleValue value, std::string* out, bool* matched) {
  *matched = true;
  if (value.isString()) {
    JS::RootedString text(cx, value.toString());
    return ToUTF8(cx, text, out);
  }
  JSObject* source = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
  const bool view = source && JS_IsArrayBufferViewObject(source);
  if (!source || (!view && !JS::IsArrayBufferObject(source))) { *matched = false; return true; }
  JS::AutoCheckCannotGC noGC(cx);
  bool shared = false;
  const size_t length = view ? JS_GetArrayBufferViewByteLength(source) : JS::GetArrayBufferByteLength(source);
  const uint8_t* bytes = view ? static_cast<const uint8_t*>(JS_GetArrayBufferViewData(source, &shared, noGC))
                              : JS::GetArrayBufferData(source, &shared, noGC);
  out->assign(reinterpret_cast<const char*>(bytes), length);
  return true;
}
void WriteAt(Writer& w, const std::string& bytes) {
  if (w.position > w.data.size()) w.data.resize(w.position, '\0');
  if (w.position + bytes.size() > w.data.size()) w.data.resize(w.position + bytes.size());
  std::memcpy(&w.data[w.position], bytes.data(), bytes.size());
  w.position += bytes.size();
}
bool Number(JSContext* cx, JS::HandleObject object, const char* name, bool* present, double* out) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, object, name, &value)) return false;
  *present = !value.isUndefined();
  return !*present || JS::ToNumber(cx, value, out);
}
// The sink's write(chunk): bytes, a string, a Blob (through its arrayBuffer()), or WriteParams.
bool SinkWrite(JSContext* cx, unsigned argc, JS::Value* vp);
bool BlobArrived(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Writer* w = WriterOf(args);
  std::string bytes;
  bool matched = false;
  if (!w || !Bytes(cx, args.get(0), &bytes, &matched)) return false;
  WriteAt(*w, bytes);
  args.rval().setUndefined();
  return true;
}
bool SinkWrite(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Of(args);
  Writer* w = WriterOf(args);
  if (!w) return false;
  JS::RootedValue chunk(cx, args.get(0));
  std::string bytes;
  bool matched = false;
  if (!Bytes(cx, chunk, &bytes, &matched)) return false;
  if (matched) { WriteAt(*w, bytes); args.rval().setUndefined(); return true; }
  if (!chunk.isObject()) return Error(cx, "FileSystemWritableFileStream.write: invalid data");
  JS::RootedObject object(cx, &chunk.toObject());
  JS::RootedValue type(cx), arrayBuffer(cx);
  if (!JS_GetProperty(cx, object, "type", &type) || !JS_GetProperty(cx, object, "arrayBuffer", &arrayBuffer))
    return false;
  std::string kind;
  if (type.isString()) { JS::RootedString text(cx, type.toString()); if (!ToUTF8(cx, text, &kind)) return false; }
  if (kind == "write" || kind == "seek" || kind == "truncate") {
    double number = 0;
    bool present = false;
    if (kind == "truncate") {
      if (!Number(cx, object, "size", &present, &number)) return false;
      if (!present) return ThrowDOM(s, "SyntaxError", "Missing size");
      w->data.resize(size_t(number), '\0');
      if (w->position > w->data.size()) w->position = w->data.size();
      args.rval().setUndefined();
      return true;
    }
    if (!Number(cx, object, "position", &present, &number)) return false;
    if (present) w->position = size_t(number);
    if (kind == "seek") { args.rval().setUndefined(); return true; }
    JS::RootedValue data(cx);
    if (!JS_GetProperty(cx, object, "data", &data)) return false;
    JS::RootedValueArray<1> inner(cx);
    inner[0].set(data);
    JS::RootedValue self(cx, JS::ObjectValue(args.callee()));
    return JS::Call(cx, JS::UndefinedHandleValue, self, inner, args.rval());
  }
  if (arrayBuffer.isObject() && JS::IsCallable(&arrayBuffer.toObject())) {  // a Blob
    JS::RootedValue pending(cx);
    if (!JS::Call(cx, chunk, arrayBuffer, JS::HandleValueArray::empty(), &pending) || !pending.isObject())
      return false;
    JSFunction* then = js::NewFunctionWithReserved(cx, BlobArrived, 1, 0, "write");
    if (!then) return false;
    JS::RootedObject thenObject(cx, JS_GetFunctionObject(then));
    js::SetFunctionNativeReserved(thenObject, 0, js::GetFunctionNativeReserved(&args.callee(), 0));
    js::SetFunctionNativeReserved(thenObject, 1, js::GetFunctionNativeReserved(&args.callee(), 1));
    JS::RootedObject promise(cx, &pending.toObject());
    JS::RootedObject chained(cx, JS::CallOriginalPromiseThen(cx, promise, thenObject, nullptr));
    if (!chained) return false;
    args.rval().setObject(*chained);
    return true;
  }
  return Error(cx, "FileSystemWritableFileStream.write: invalid data");
}
bool SinkClose(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Writer* w = WriterOf(args);
  if (!w) return false;
  if (!OpfsTree::Live(*w->node)) return ThrowDOM(Of(args), "NotFoundError", "Entry not found");
  w->node->data = w->data;
  w->node->lastModified = Now();
  args.rval().setUndefined();
  return true;
}
bool SinkAbort(JSContext*, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  args.rval().setUndefined();
  return true;
}
}  // namespace

bool OpfsCreateWritable(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Of(args);
  auto* node = OpfsNodeOf(args.thisv(), &kOpfsFileClass);
  if (!node) { WebKit::FailBrand(cx, args, "FileSystemFileHandle"); return OpfsRejectPending(cx, args.rval()); }
  s.Observe("FileSystemFileHandle", "createWritable", 0);
  if (!OpfsTree::Live(**node)) return OpfsRejectStatus(s, OpfsStatus::NotFound, args.rval());
  if ((*node)->exclusiveLock)
    return OpfsReject(s, "NoModificationAllowedError", "No modification allowed", args.rval());
  bool keep = false;
  if (args.get(0).isObject()) {
    JS::RootedObject options(cx, &args.get(0).toObject());
    JS::RootedValue value(cx);
    if (!JS_GetProperty(cx, options, "keepExistingData", &value)) return OpfsRejectPending(cx, args.rval());
    keep = JS::ToBoolean(value);
  }
  auto* writer = new (std::nothrow) Writer{*node, keep ? (*node)->data : std::string(), 0};
  JS::RootedObject holder(cx, JS_NewObject(cx, &kWriterClass));
  if (!writer || !holder) { delete writer; JS_ReportOutOfMemory(cx); return false; }
  JS::SetReservedSlot(holder, 0, JS::PrivateValue(writer));
  JS::RootedObject sink(cx, JS_NewPlainObject(cx));
  if (!sink) return false;
  const struct { const char* name; JSNative native; } members[] = {
      {"write", SinkWrite}, {"close", SinkClose}, {"abort", SinkAbort}};
  for (const auto& member : members) {
    JSFunction* fn = js::NewFunctionWithReserved(cx, member.native, 1, 0, member.name);
    if (!fn) return false;
    JS::RootedObject object(cx, JS_GetFunctionObject(fn));
    js::SetFunctionNativeReserved(object, 0, JS::PrivateValue(&s));
    js::SetFunctionNativeReserved(object, 1, JS::ObjectValue(*holder));
    if (!JS_DefineProperty(cx, sink, member.name, object, 0)) return false;
  }
  JS::RootedValue streamCtor(cx);
  JS::RootedValueArray<1> ctorArgs(cx);
  ctorArgs[0].setObject(*sink);
  JS::RootedObject stream(cx), prototype(cx, s.opfsStore->prototype(kOpfsWritable));
  if (!JS_GetProperty(cx, s.page, "WritableStream", &streamCtor) ||
      !JS::Construct(cx, streamCtor, ctorArgs, &stream) || !JS_SetPrototype(cx, stream, prototype))
    return OpfsRejectPending(cx, args.rval());
  if (!s.opfsStore->MarkWritable(stream)) return false;
  JS::RootedValue result(cx, JS::ObjectValue(*stream));
  return OpfsResolve(cx, result, args.rval());
}

namespace {
bool StreamMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Of(args);
  const int member = WebKit::Slot(args);  // 0 write, 1 seek, 2 truncate
  if (!s.opfsStore->IsWritable(args.thisv())) {
    WebKit::FailBrand(cx, args, "FileSystemWritableFileStream");
    return OpfsRejectPending(cx, args.rval());
  }
  JS::RootedValue data(cx, args.get(0));
  if (member) {
    JS::RootedObject params(cx, JS_NewPlainObject(cx));
    JS::RootedString type(cx, JS_NewStringCopyZ(cx, member == 1 ? "seek" : "truncate"));
    JS::RootedValue typeValue(cx), number(cx);
    if (!params || !type) return false;
    typeValue.setString(type);
    double value = 0;
    if (!JS::ToNumber(cx, args.get(0), &value)) return OpfsRejectPending(cx, args.rval());
    number.setNumber(value);
    if (!JS_DefineProperty(cx, params, "type", typeValue, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, params, member == 1 ? "position" : "size", number, JSPROP_ENUMERATE)) return false;
    data.setObject(*params);
  }
  JS::RootedObject self(cx, &args.thisv().toObject());
  JS::RootedValue writer(cx), promise(cx), ignored(cx);
  JS::RootedValueArray<1> writeArgs(cx);
  writeArgs[0].set(data);
  if (!JS_CallFunctionName(cx, self, "getWriter", JS::HandleValueArray::empty(), &writer) || !writer.isObject())
    return OpfsRejectPending(cx, args.rval());
  JS::RootedObject writerObject(cx, &writer.toObject());
  if (!JS_CallFunctionName(cx, writerObject, "write", writeArgs, &promise) ||
      !JS_CallFunctionName(cx, writerObject, "releaseLock", JS::HandleValueArray::empty(), &ignored))
    return OpfsRejectPending(cx, args.rval());
  args.rval().set(promise);
  return true;
}
}  // namespace

bool InstallOpfsWritable(State& s, JS::HandleObject prototype) {
  const char* names[] = {"write", "seek", "truncate"};
  for (int i = 0; i < 3; ++i) {
    JS::RootedObject fn(s.cx, s.Function(StreamMember, names[i], 1, i));
    if (!fn || !JS_DefineProperty(s.cx, prototype, names[i], fn, JSPROP_ENUMERATE)) return false;
  }
  return true;
}
}  // namespace ruyijs::browser_environment
