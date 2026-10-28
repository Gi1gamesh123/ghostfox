#ifndef RUYIJS_BROWSER_ENVIRONMENT_STORAGE_MANAGER_H
#define RUYIJS_BROWSER_ENVIRONMENT_STORAGE_MANAGER_H

namespace ruyijs::browser_environment {
struct State;

// Firefox's navigator.storage surface. The Browser Environment owns the
// realm-local StorageManager instance; quota data follows the desktop Firefox
// default and methods return settled Promises. getDirectory() resolves the
// in-memory OPFS root owned by storage_opfs.cpp, so Worker access-handle
// flush timing matches an in-memory browser store.
bool InstallStorageManager(State& state);
}

#endif
