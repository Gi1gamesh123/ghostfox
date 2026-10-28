// The Window-only legacy File and Directory Entries interfaces (FileSystem,
// FileSystemEntry, -FileEntry, -DirectoryEntry, -DirectoryReader). Firefox creates
// their instances only for dropped or webkitdirectory files, which this runtime
// never produces (DEVIATIONS D145): the interface objects exist with illegal
// constructors and every member fails its WebIDL brand check.
#include "browser_environment/storage_opfs.h"
#include "browser_environment/state.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::browser_environment {
namespace {
struct Face { const char* name; const char* parent; const char* methods[3]; unsigned lengths[3];
              const char* getters[5]; };
const Face kFaces[] = {
    {"FileSystem", nullptr, {}, {}, {"name", "root"}},
    {"FileSystemEntry", nullptr, {"getParent"}, {0}, {"isFile", "isDirectory", "name", "fullPath", "filesystem"}},
    {"FileSystemFileEntry", "FileSystemEntry", {"file"}, {1}, {}},
    {"FileSystemDirectoryEntry", "FileSystemEntry", {"createReader", "getFile", "getDirectory"}, {0, 0, 0}, {}},
    {"FileSystemDirectoryReader", nullptr, {"readEntries"}, {1}, {}},
};
bool Brand(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  return WebKit::FailBrand(cx, args, kFaces[WebKit::Slot(args)].name);
}
}  // namespace

bool InstallOpfsEntries(State& s) {
  JSContext* cx = s.cx;
  JS::RootedObject objectProto(cx, JS::GetRealmObjectPrototype(cx)), none(cx);
  JS::RootedObject entryProto(cx), entryCtor(cx);
  for (int index = 0; index < int(std::size(kFaces)); ++index) {
    const Face& face = kFaces[index];
    const bool derived = face.parent != nullptr;
    JS::RootedObject prototype(cx);
    if (!OpfsInterface(s, face.name, derived ? entryProto : objectProto, derived ? entryCtor : none, &prototype))
      return false;
    for (int i = 0; i < 3 && face.methods[i]; ++i) {
      JS::RootedObject fn(cx, s.Function(Brand, face.methods[i], face.lengths[i], index));
      if (!fn || !JS_DefineProperty(cx, prototype, face.methods[i], fn, JSPROP_ENUMERATE)) return false;
    }
    for (int i = 0; i < 5 && face.getters[i]; ++i)
      if (!s.Accessor(prototype, face.getters[i], index, JSPROP_ENUMERATE, Brand)) return false;
    if (index == 1) {
      JS::RootedValue ctor(cx);
      if (!JS_GetProperty(cx, s.page, face.name, &ctor)) return false;
      entryProto = prototype;
      entryCtor = &ctor.toObject();
    }
  }
  return true;
}
}  // namespace ruyijs::browser_environment
