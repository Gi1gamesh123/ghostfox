// Event handler content attributes (docs/owners/DOCUMENT_MODEL.md). Setting
// `onclick="..."` on an element of this realm's main document makes the same
// IDL handler hold the uncompiled source; it is compiled on first read or
// dispatch with Firefox's scope chain element -> form owner -> document and
// the parameter `event` (`evt` for SVG), and removing the attribute clears it.
// body/frameset window-reflected handler attributes (WindowEventHandlers plus
// onblur/onerror/onfocus/onload/onresize/onscroll) instead set their Window's
// handler: the source is compiled with the window's global scope and stored
// through the Window's own accessor (Browser Environment's single handler
// owner), never on the element.
#include "document_model/state.h"
#include "document_model/event_handler_names.h"
#include "document_model/body_frameset_handlers.h"
#include "runtime/exception_capture.h"
#include "browser_environment/environment.h"
#include "interaction/realm.h"
#include "js/CompilationAndEvaluation.h"
#include "js/EnvironmentChain.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/SourceText.h"
#include "js/StableStringChars.h"
#include <cstring>
#include <string>

namespace ruyijs::document_model {
bool CompileWindowEventHandler(State&, const char*, JS::HandleString, JS::MutableHandleObject);
namespace {
constexpr char16_t kSvgNamespace[] = u"http://www.w3.org/2000/svg";
// One attribute name against a handler-name list; returns the canonical name.
const char* MatchHandler(DomStringView name, const char* const* names, size_t count) {
  if (name.size() < 3 || name[0] != u'o' || name[1] != u'n') return nullptr;
  for (size_t i = 0; i < count; ++i) {
    const char* candidate = names[i];
    if (std::strlen(candidate) != name.size()) continue;
    bool same = true;
    for (size_t j = 0; same && j < name.size(); ++j) same = char16_t(candidate[j]) == name[j];
    if (same) return candidate;
  }
  return nullptr;
}
// The content attribute names that map to an element's own handler.
const char* AttributeHandler(DomStringView name) {
  return MatchHandler(name, kGlobalEventHandlerNames, kGlobalEventHandlerCount);
}
// The content attribute names body/frameset reflect to their Window.
const char* BodyFramesetHandler(DomStringView name) {
  return MatchHandler(name, kBodyFramesetWindowHandlerNames, kBodyFramesetWindowHandlerCount);
}
bool Scripted(const NodeSnapshot& node) {
  if (node.kind != NodeKind::Element) return false;
  if (node.is_html_element || !node.namespace_uri) return node.is_html_element;
  return *node.namespace_uri == kSvgNamespace ||
         *node.namespace_uri == u"http://www.w3.org/1998/Math/MathML";
}
// Gives `key` the attribute's current value (or none) as its handler.
bool Apply(State& state, NodeKey key, const char* handler) {
  JSContext* cx = state.cx;
  std::optional<DomString> text;
  std::u16string attribute(handler, handler + std::strlen(handler));
  if (!state.Check(state.model.GetAttribute(key, attribute, &text))) return false;
  JS::RootedValue wrapped(cx);
  if (!state.Wrap(key, &wrapped) || !wrapped.isObject()) return false;
  JS::RootedObject wrapper(cx, &wrapped.toObject());
  JS::RootedValue value(cx, JS::NullValue());
  if (text) {
    JS::RootedString source(cx, JS_NewUCStringCopyN(cx, text->data(), text->size()));
    if (!source) return false;
    // As the donor's EventListenerManager::SetEventHandler, CSP is checked when
    // the attribute is set; a blocked body leaves no handler.
    bool allowed = true;
    if (!SecurityAllowsHandler(state, wrapper, source, &allowed)) return false;
    if (allowed) value.setString(source);
  }
  return WriteEventHandler(state, wrapper, handler, value);
}
// A body/frameset window-reflected handler that does not compile is reported as
// an uncaught script error on the Window and leaves no handler, the same report
// the element path's ReadEventHandler gives its compile failures. This path runs
// synchronously inside setAttribute, so it fires the window error event instead
// of capturing into the host request: setAttribute itself must not throw.
bool ReportWindowHandlerFailure(State& state) {
  JSContext* cx = state.cx;
  JS::RootedValue error(cx, JS::UndefinedValue());
  if (JS_IsExceptionPending(cx)) (void)JS_GetPendingException(cx, &error);
  JS_ClearPendingException(cx);
  std::string file = "about:blank";
  JS::RootedValue url(cx);
  if (state.environment.URL(&url) && url.isString()) {
    JS::RootedString href(cx, url.toString());
    JS::UniqueChars bytes = JS_EncodeStringToUTF8(cx, href);
    if (bytes) file = bytes.get();
  }
  ExceptionReport report = DescribeException(cx, error, nullptr);
  const std::string message = report.name.empty() ? report.message
                                                  : report.name + ": " + report.message;
  const bool reported =
      state.interaction.FireError(state.page, error, message.c_str(), file.c_str(), 0, 0);
  JS_ClearPendingException(cx);
  return reported;
}
// Gives the Window the attribute's current value as a compiled handler. The
// Window's accessor stores object/null only, so the source compiles eagerly
// with the same eventHandler introduction as the element path, but a global
// (window-only) scope chain, matching Firefox's forwarded window handlers.
bool ApplyWindowHandler(State& state, NodeKey key, const char* handler) {
  JSContext* cx = state.cx;
  std::optional<DomString> text;
  std::u16string attribute(handler, handler + std::strlen(handler));
  if (!state.Check(state.model.GetAttribute(key, attribute, &text))) return false;
  JS::RootedValue value(cx, JS::NullValue());
  if (text) {
    JS::RootedString source(cx, JS_NewUCStringCopyN(cx, text->data(), text->size()));
    if (!source) return false;
    JS::RootedValue wrapped(cx);
    if (!state.Wrap(key, &wrapped) || !wrapped.isObject()) return false;
    JS::RootedObject wrapper(cx, &wrapped.toObject());
    bool allowed = true;
    if (!SecurityAllowsHandler(state, wrapper, source, &allowed)) return false;
    if (allowed) {
      JS::RootedObject compiled(cx);
      if (!CompileWindowEventHandler(state, handler, source, &compiled)) {
        if (JS_IsThrowingOutOfMemory(cx)) return false;
        if (!ReportWindowHandlerFailure(state)) return false;
      } else {
        value.setObject(*compiled);
      }
    }
  }
  return JS_SetProperty(cx, state.page, handler, value);
}
void Settle(State& state) {
  // A sink has no caller to return an error to; the failure is reported.
  if (JS_IsExceptionPending(state.cx) && !JS_IsThrowingOutOfMemory(state.cx)) {
    CaptureOperationFailure(state.cx);
    JS_ClearPendingException(state.cx);
  }
}
}  // namespace
void EventHandlerAttributeChanged(State& state, NodeKey key, DomStringView name) {
  if (key.document != state.main.document) return;
  NodeSnapshot node;
  if (state.model.Inspect(key, &node).code != ErrorCode::None || !Scripted(node)) return;
  JSAutoRealm realm(state.cx, state.page);
  const char* handler = BodyFramesetHandler(name);
  if (handler && IsBodyFramesetInterface(node.interface_name)) {
    if (!ApplyWindowHandler(state, key, handler)) Settle(state);
    return;
  }
  handler = AttributeHandler(name);
  if (!handler) return;
  if (!Apply(state, key, handler)) Settle(state);
}
void EventHandlerAttributesInserted(State& state, const std::vector<NodeKey>& inserted) {
  JSAutoRealm realm(state.cx, state.page);
  for (NodeKey scope : inserted) {
    if (scope.document != state.main.document) continue;
    std::vector<NodeKey> nodes;
    if (state.model.Subtree(scope, &nodes).code != ErrorCode::None) continue;
    for (NodeKey key : nodes) {
      NodeSnapshot node;
      if (state.model.Inspect(key, &node).code != ErrorCode::None || !Scripted(node)) continue;
      std::vector<AttributeSnapshot> attributes;
      if (state.model.Attributes(key, &attributes).code != ErrorCode::None) continue;
      for (const auto& attribute : attributes) {
        if (!attribute.namespace_uri.empty()) continue;
        const char* handler = BodyFramesetHandler(attribute.name);
        if (handler && IsBodyFramesetInterface(node.interface_name)) {
          if (!ApplyWindowHandler(state, key, handler)) Settle(state);
          continue;
        }
        handler = AttributeHandler(attribute.name);
        if (!handler) continue;
        // A node that moves keeps the handler its attribute already gave it.
        JS::RootedValue wrapped(state.cx);
        bool present = false;
        if (!state.Wrap(key, &wrapped) || !wrapped.isObject()) { Settle(state); break; }
        JS::RootedObject wrapper(state.cx, &wrapped.toObject());
        if (!HasEventHandler(wrapper, handler, state.cx, &present)) { Settle(state); break; }
        if (!present && !Apply(state, key, handler)) Settle(state);
      }
    }
  }
}
bool CompileSource(State& state, JS::EnvironmentChain& chain, const char* name,
                   const char* argument, JS::HandleString source,
                   JS::MutableHandleObject result) {
  JSContext* cx = state.cx;
  JS::RootedValue url(cx);
  std::string file = "about:blank";
  if (state.environment.URL(&url) && url.isString()) {
    JS::RootedString href(cx, url.toString());
    JS::UniqueChars bytes = JS_EncodeStringToUTF8(cx, href);
    if (bytes) file = bytes.get();
  }
  JS::AutoStableStringChars characters(cx);
  if (!characters.initTwoByte(cx, source)) return false;
  JS::SourceText<char16_t> text;
  if (!text.init(cx, characters.twoByteChars(), characters.length(), JS::SourceOwnership::Borrowed))
    return false;
  JS::CompileOptions options(cx);
  // As the donor's EventListenerManager: line 0, because the compiled function
  // header takes a line of its own, so the attribute body starts at line 1.
  options.setIntroductionType("eventHandler").setFileAndLine(file.c_str(), 0);
  const char* arguments[] = {argument};
  JSFunction* function = JS::CompileFunction(cx, chain, options, name, 1, arguments, text);
  if (!function) return false;
  result.set(JS_GetFunctionObject(function));
  return true;
}
bool CompileWindowEventHandler(State& state, const char* name, JS::HandleString source,
                               JS::MutableHandleObject result) {
  JS::EnvironmentChain chain(state.cx, JS::SupportUnscopables::Yes);
  return CompileSource(state, chain, name, "event", source, result);
}
bool CompileEventHandler(State& state, JS::HandleObject wrapper, const char* name,
                         JS::HandleString source, JS::MutableHandleObject result) {
  JSContext* cx = state.cx;
  auto* entry = static_cast<Wrapper*>(JS::GetReservedSlot(wrapper, 0).toPrivate());
  if (!entry || entry->dead) return TypeError(cx, "Event handler element is gone");
  NodeSnapshot node;
  if (!state.Check(state.model.Inspect(entry->key, &node))) return false;
  JS::EnvironmentChain chain(cx, JS::SupportUnscopables::Yes);
  JS::RootedValue scope(cx);
  if (!chain.append(wrapper)) return false;
  NodeKey form;
  if (node.kind == NodeKind::Element &&
      state.model.FormOwner(entry->key, &form).code == ErrorCode::None && (form.document || form.node)) {
    if (!state.Wrap(form, &scope) || (scope.isObject() && !chain.append(&scope.toObject()))) return false;
  }
  if (node.kind == NodeKind::Element) {
    if (!state.Wrap(node.owner_document, &scope) || (scope.isObject() && !chain.append(&scope.toObject())))
      return false;
  }
  const bool svg = node.namespace_uri && *node.namespace_uri == kSvgNamespace;
  return CompileSource(state, chain, name, svg ? "evt" : "event", source, result);
}
}  // namespace ruyijs::document_model
