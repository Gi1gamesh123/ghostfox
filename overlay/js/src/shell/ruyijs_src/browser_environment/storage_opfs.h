#ifndef RUYIJS_BROWSER_ENVIRONMENT_STORAGE_OPFS_H
#define RUYIJS_BROWSER_ENVIRONMENT_STORAGE_OPFS_H
#include <memory>
#include <string>
#include "domain/browser_environment/opfs_tree.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs::browser_environment {
struct State;
using OpfsNode = ruyijs::domain::browser_environment::OpfsNode;
using OpfsTree = ruyijs::domain::browser_environment::OpfsTree;
using OpfsStatus = ruyijs::domain::browser_environment::OpfsStatus;

// The realm's origin private file system behind navigator.storage.getDirectory()
// (docs/owners/BROWSER_ENVIRONMENT.md, "OPFS"). The Browser Environment State owns
// one store; handles are realm-local objects holding a shared_ptr to their node.
enum OpfsFace { kOpfsHandle, kOpfsFile, kOpfsDirectory, kOpfsWritable, kOpfsSync, kOpfsIterator,
                kOpfsFaceCount };
class OpfsStore {
 public:
  OpfsStore(JSContext*, State&);
  OpfsTree& tree() { return tree_; }
  State& state() { return state_; }
  JSObject* prototype(OpfsFace face) const { return prototypes_[face]; }
  void SetPrototype(OpfsFace face, JSObject* prototype) { prototypes_[face] = prototype; }
  // A FileSystemFileHandle or FileSystemDirectoryHandle of this realm for `node`.
  JSObject* NewHandle(const std::shared_ptr<OpfsNode>& node);
  void Observe(const char* iface, const char* member, int type) const;
  // The FileSystemWritableFileStream brand: streams createWritable() made.
  bool MarkWritable(JS::HandleObject stream);
  bool IsWritable(JS::HandleValue value);

 private:
  JSContext* cx_;
  State& state_;
  OpfsTree tree_;
  JS::PersistentRootedObject prototypes_[kOpfsFaceCount];
  JS::PersistentRootedObject writables_;
};
extern const JSClass kOpfsFileClass, kOpfsDirectoryClass, kOpfsIteratorClass;
// The node behind a file/directory handle (or a sync access handle), else null.
std::shared_ptr<OpfsNode>* OpfsNodeOf(JS::HandleValue value, const JSClass* clasp);
OpfsStore* OpfsStoreOf(const JS::CallArgs& args);
// Promise helpers: resolve with a value; reject with a DOMException `name` or
// with the pending exception (a WebIDL brand or conversion failure).
bool OpfsResolve(JSContext*, JS::HandleValue, JS::MutableHandleValue rval);
bool OpfsReject(State&, const char* name, const char* message, JS::MutableHandleValue rval);
bool OpfsRejectPending(JSContext*, JS::MutableHandleValue rval);
bool OpfsRejectStatus(State&, OpfsStatus, JS::MutableHandleValue rval);
JSObject* OpfsNewObject(State&, const JSClass*, OpfsFace, const std::shared_ptr<OpfsNode>&);
// Installers: the handle interfaces (Window and every Worker kind; the sync access
// handle in dedicated Workers only) and the Window-only legacy entries API.
bool InstallOpfs(State&);
// One WebIDL interface object: illegal constructor, prototype, toStringTag, global.
bool OpfsInterface(State&, const char* name, JS::HandleObject parentProto, JS::HandleObject parentCtor,
                   JS::MutableHandleObject prototype);
bool InstallOpfsHandleMembers(State&, JS::HandleObject handle, JS::HandleObject file,
                              JS::HandleObject directory);
bool InstallOpfsWritable(State&, JS::HandleObject prototype);
bool InstallOpfsSync(State&, JS::HandleObject prototype);
bool InstallOpfsEntries(State&);
// getFile() (storage_opfs_file.cpp), createWritable() (storage_opfs_writable.cpp),
// createSyncAccessHandle() (storage_opfs_sync.cpp).
bool OpfsCreateWritable(JSContext*, unsigned, JS::Value*);
bool OpfsGetFile(JSContext*, unsigned, JS::Value*);
bool OpfsCreateSyncAccessHandle(JSContext*, unsigned, JS::Value*);
}
#endif
