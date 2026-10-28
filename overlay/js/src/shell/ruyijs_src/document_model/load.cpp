#include "document_model/state.h"
#include "js/Realm.h"
#include "js/Object.h"

namespace ruyijs::document_model {
static Wrapper* Script(State& state, JS::HandleObject object) {
  Wrapper* entry = object ? state.Find(object) : nullptr;
  if (!entry) TypeError(state.cx, "Page load script is not a node of this realm");
  return entry;
}
JSObject* Realm::document() {
  State& state = *state_;
  JS::RootedValue value(state.cx);
  return state.Wrap(state.main, &value) && value.isObject() ? &value.toObject() : nullptr;
}
bool Realm::BeginLoad(JS::HandleString markup) {
  State& state = *state_;
  ++state.scriptGeneration;
  JSAutoRealm realm(state.cx, state.page);
  JS::RootedValue value(state.cx, JS::StringValue(markup));
  DomString units;
  if (!String(state.cx, value, &units)) return false;
  const Status status = state.model.BeginParse(state.main, units);
  if (status.code == ErrorCode::None)
    if (auto* document = state.Find(state.main)) JS::SetReservedSlot(document->object, 7, JS::UndefinedValue());
  return state.Check(status);
}
bool Realm::NextScript(JS::MutableHandleObject script) {
  State& state = *state_;
  JSAutoRealm realm(state.cx, state.page);
  NodeKey key;
  JS::RootedValue value(state.cx);
  if (!state.Check(state.model.ResumeParse(state.main, &key)) || !state.Wrap(key, &value)) return false;
  bool execute=false,external=false;
  if(key.node && !state.Check(state.model.PrepareScript(key,true,&execute,&external)))return false;
  script.set(value.isObject() ? &value.toObject() : nullptr);
  return true;
}
bool Realm::SetReadyState(ReadyState ready) {
  return state_->Check(state_->model.SetReadyState(state_->main, ready));
}
bool Realm::SetCurrentScript(JS::HandleObject script) {
  State& state = *state_;
  Wrapper* entry = script ? Script(state, script) : nullptr;
  if (script && !entry) return false;
  return state.Check(state.model.SetCurrentScript(state.main, entry ? entry->key : NodeKey{}));
}
bool Realm::ScriptAttribute(JS::HandleObject script, const char16_t* name, JS::MutableHandleValue result) {
  State& state = *state_;
  Wrapper* entry = Script(state, script);
  std::optional<DomString> value;
  return entry && state.Check(state.model.GetAttribute(entry->key, name, &value)) &&
         Value(state.cx, value, result);
}
bool Realm::Attribute(JS::HandleObject element, const char16_t* name, JS::MutableHandleValue result) {
  State& state = *state_;
  Wrapper* entry = element ? state.Find(element) : nullptr;
  if (!entry) return TypeError(state.cx, "Element is not a node of this realm");
  std::optional<DomString> value;
  return state.Check(state.model.GetAttribute(entry->key, name, &value)) &&
         Value(state.cx, value, result);
}
bool Realm::ScriptPosition(JS::HandleObject script, uint32_t* line, uint32_t* column) {
  State& state = *state_;
  Wrapper* entry = Script(state, script);
  return entry && state.Check(state.model.ScriptPosition(entry->key, line, column));
}
bool Realm::ScriptText(JS::HandleObject script, JS::MutableHandleValue result) {
  State& state = *state_;
  Wrapper* entry = Script(state, script);
  std::optional<DomString> text;
  return entry && state.Check(state.model.TextContent(entry->key, &text)) &&
         Value(state.cx, text, result);
}
}
