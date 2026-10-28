// HTMLInputElement.files: a FileList for a file input (the same object until it is
// replaced), null for every other type. The FileList interface and factory belong to
// the Web Platform owner (FileListPort); the selection itself stays empty here, as no
// file picker exists, unless script assigns a FileList.
#include "document_model/html_control.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/WeakMap.h"

namespace ruyijs::document_model {
namespace {
bool IsFileInput(State& state, const Wrapper& entry, bool* file) {
  DomString type;
  if (!state.Check(state.model.InputText(entry.key, u"type", &type))) return false;
  *file = type == u"file";
  return true;
}
bool Store(State& state, JS::MutableHandleObject map) {
  if (!state.inputFiles) state.inputFiles = JS::NewWeakMapObject(state.cx);
  map.set(state.inputFiles);
  return map != nullptr;
}
}  // namespace

bool InputFiles(Wrapper& entry, JS::MutableHandleValue result) {
  State& state = *entry.owner;
  JSContext* cx = state.cx;
  bool file = false;
  result.setNull();
  if (!IsFileInput(state, entry, &file)) return false;
  if (!file) return ResetInputFiles(entry);
  JS::RootedObject map(cx);
  JS::RootedValue key(cx, JS::ObjectValue(*entry.object));
  if (!Store(state, &map) || !JS::GetWeakMapEntry(cx, map, key, result)) return false;
  if (result.isObject()) return true;
  if (!state.fileList) return TypeError(cx, "FileList factory is not installed");
  JS::RootedObject list(cx), none(cx);
  if (!state.fileList(state.fileListContext, none, &list)) return false;
  result.setObject(*list);
  return JS::SetWeakMapEntry(cx, map, key, result);
}

// A type change clears the selection: the next file-type read makes a new FileList.
bool ResetInputFiles(Wrapper& entry) {
  State& state = *entry.owner;
  if (!state.inputFiles) return true;
  JS::RootedObject map(state.cx, state.inputFiles);
  JS::RootedValue key(state.cx, JS::ObjectValue(*entry.object));
  return JS::SetWeakMapEntry(state.cx, map, key, JS::UndefinedHandleValue);  // reads accept objects only
}
// Firefox: assigning null is ignored; a FileList replaces the selection of a file input.
bool SetInputFiles(Wrapper& entry, JS::HandleValue value) {
  State& state = *entry.owner;
  JSContext* cx = state.cx;
  if (value.isNull()) return true;
  if (!value.isObject()) return TypeError(cx, "HTMLInputElement.files setter: Value being assigned is not an object.");
  bool file = false;
  if (!IsFileInput(state, entry, &file)) return false;
  if (!file) return true;
  JS::RootedObject map(cx);
  JS::RootedValue key(cx, JS::ObjectValue(*entry.object)), list(cx, value);
  return Store(state, &map) && JS_WrapValue(cx, &list) && JS::SetWeakMapEntry(cx, map, key, list);
}
}  // namespace ruyijs::document_model
