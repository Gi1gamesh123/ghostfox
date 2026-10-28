// The Blob and File installers. Each defines exactly one constructor with its
// own class and prototype; File.prototype's [[Prototype]] is Blob.prototype, so
// instanceof and the shared Blob receiver brand both hold.
#include "web_platform/state.h"

#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::web_platform {
bool InstallBlob(State& state, PageHelpers&) {
  JSContext* cx = state.cx;
  WebKit& kit = state.kit;
  JS::RootedObject objectProto(cx, JS::GetRealmObjectPrototype(cx));
  return objectProto &&
         kit.Define("Blob", &kBlobClass, objectProto, BlobConstructor, &state.blob) &&
         kit.Method(state.blob, "Blob", "text", BlobMethod, 0, kBlobText) &&
         kit.Method(state.blob, "Blob", "arrayBuffer", BlobMethod, 0, kBlobArrayBuffer) &&
         kit.Method(state.blob, "Blob", "slice", BlobMethod, 0, kBlobSlice) &&
         kit.Method(state.blob, "Blob", "stream", BlobMethod, 0, kBlobStream) &&
         kit.Method(state.blob, "Blob", "bytes", BlobMethod, 0, kBlobBytes) &&
         kit.Accessor(state.blob, "Blob", "size", BlobGetter, nullptr, kBlobSizeField) &&
         kit.Accessor(state.blob, "Blob", "type", BlobGetter, nullptr, kBlobTypeField);
}

bool InstallFile(State& state, PageHelpers&) {
  WebKit& kit = state.kit;
  return kit.Define("File", &kFileClass, state.blob.prototype, FileConstructor,
                    &state.file) &&
         JS_SetPrototype(state.cx, state.file.constructor, state.blob.constructor) &&
         JS_DefineProperty(state.cx, state.file.constructor, "length", 2, JSPROP_READONLY) &&
         kit.Accessor(state.file, "File", "name", FileGetter, nullptr, kFileNameField) &&
         kit.Accessor(state.file, "File", "lastModified", FileGetter, nullptr,
                      kFileLastModifiedField) &&
         kit.Accessor(state.file, "File", "webkitRelativePath", FileGetter, nullptr,
                      kFileRelativePathField);
}
}  // namespace ruyijs::web_platform
