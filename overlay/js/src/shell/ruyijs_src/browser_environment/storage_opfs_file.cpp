// FileSystemFileHandle.getFile(): the Web Platform File built through the realm's own
// File constructor, typed by extension as Firefox's MIME service does for common types.
#include "browser_environment/storage_opfs.h"
#include "browser_environment/state.h"
#include "runtime/runtime.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/ArrayBuffer.h"
#include "js/CallAndConstruct.h"
#include "js/PropertyAndElement.h"
#include "js/ValueArray.h"
#include <cstring>

namespace ruyijs::browser_environment {
namespace {
State& Of(const JS::CallArgs& args) {
  return *static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
}
const char* MimeOf(const std::string& name) {
  static const char* const table[][2] = {
      {"txt", "text/plain"}, {"html", "text/html"}, {"htm", "text/html"}, {"css", "text/css"},
      {"js", "text/javascript"}, {"mjs", "text/javascript"}, {"json", "application/json"},
      {"xml", "text/xml"}, {"png", "image/png"}, {"jpg", "image/jpeg"}, {"jpeg", "image/jpeg"},
      {"gif", "image/gif"}, {"svg", "image/svg+xml"}, {"webp", "image/webp"}, {"pdf", "application/pdf"},
      {"wasm", "application/wasm"}, {"bin", "application/octet-stream"}, {"zip", "application/zip"}};
  const size_t dot = name.rfind('.');
  if (dot == std::string::npos) return "";
  std::string extension = name.substr(dot + 1);
  for (char& c : extension) if (c >= 'A' && c <= 'Z') c = char(c + 32);
  for (const auto& entry : table) if (extension == entry[0]) return entry[1];
  return "";
}
}  // namespace

bool OpfsGetFile(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Of(args);
  auto* node = OpfsNodeOf(args.thisv(), &kOpfsFileClass);
  if (!node) { WebKit::FailBrand(cx, args, "FileSystemFileHandle"); return OpfsRejectPending(cx, args.rval()); }
  s.Observe("FileSystemFileHandle", "getFile", 0);
  if (!OpfsTree::Live(**node)) return OpfsRejectStatus(s, OpfsStatus::NotFound, args.rval());
  const OpfsNode& file = **node;
  JS::RootedObject buffer(cx, JS::NewArrayBuffer(cx, file.data.size()));
  if (!buffer) return false;
  if (!file.data.empty()) {
    JS::AutoCheckCannotGC noGC(cx);
    bool shared = false;
    std::memcpy(JS::GetArrayBufferData(buffer, &shared, noGC), file.data.data(), file.data.size());
  }
  JS::RootedValueArray<1> parts(cx);
  parts[0].setObject(*buffer);
  JS::RootedObject partList(cx, JS::NewArrayObject(cx, parts)), options(cx, JS_NewPlainObject(cx));
  JS::RootedString name(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(file.name.data(), file.name.size())));
  JS::RootedString type(cx, JS_NewStringCopyZ(cx, MimeOf(file.name)));
  JS::RootedValue fileCtor(cx), modified(cx, JS::NumberValue(file.lastModified)), typeValue(cx);
  if (!partList || !options || !name || !type || !JS_GetProperty(cx, s.page, "File", &fileCtor)) return false;
  typeValue.setString(type);
  if (!JS_DefineProperty(cx, options, "type", typeValue, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, options, "lastModified", modified, JSPROP_ENUMERATE)) return false;
  JS::RootedValueArray<3> ctorArgs(cx);
  ctorArgs[0].setObject(*partList);
  ctorArgs[1].setString(name);
  ctorArgs[2].setObject(*options);
  JS::RootedObject made(cx);
  if (!JS::Construct(cx, fileCtor, ctorArgs, &made)) return OpfsRejectPending(cx, args.rval());
  JS::RootedValue result(cx, JS::ObjectValue(*made));
  return OpfsResolve(cx, result, args.rval());
}
}  // namespace ruyijs::browser_environment
