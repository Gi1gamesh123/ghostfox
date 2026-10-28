#include "document_model/state.h"
#include "document_model/foreign_adoption.h"
#include "document_model/collections.h"
#include "document_model/traversal.h"
#include "document_model/range.h"
#include "document_model/parsing.h"
#include "document_model/xpath.h"
#include "document_model/selection.h"
#include "document_model/fragment_directive.h"
#include "browser_environment/environment.h"
#include "runtime/page_helpers.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
namespace ruyijs::document_model {
enum Field { Root, Head, Body, Ready, URL, Location, View, ContentType, Encoding,
              Active, Hidden, Visibility, Script, Title, Doctype };
enum Method { Element, Text, Comment, Id, Fragment, Attribute, ImportNode, AdoptNode,
              TreeWalker, NodeIterator, MakeRange };
// Canonical member name reported to the debug monitor. Aliased properties
// (documentURI, charset, inputEncoding) report their primary name, so one
// watch entry covers the field rather than each spelling.
static const char* FieldName(Field field) {
  switch (field) {
    case Root: return "documentElement";
    case Head: return "head";
    case Body: return "body";
    case Ready: return "readyState";
    case URL: return "URL";
    case Location: return "location";
    case View: return "defaultView";
    case ContentType: return "contentType";
    case Encoding: return "characterSet";
    case Active: return "activeElement";
    case Hidden: return "hidden";
    case Visibility: return "visibilityState";
    case Script: return "currentScript";
    case Title: return "title";
    case Doctype: return "doctype";
  }
  return "";
}
static bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto field = Field(FunctionSlot(args));
  if (field == Title) {
    Wrapper* entry = NodeReceiver(cx, args.thisv(), NodeKind::Document);
    if (!entry) return false;
    State& owner = *entry->owner;
    DomString text;
    {
      JSAutoRealm realm(cx, owner.page);
      owner.Observe("Document", "title", int(ApiAccess::Read));
      if (!owner.Check(owner.model.Title(entry->key, &text))) return false;
    }
    return Value(cx, DomStringView(text), args.rval());
  }
  State& state = *FunctionState(args);
  NodeKind kind = NodeKind::Document;
  Wrapper* entry = Receiver(state, args, &kind);
  if (!entry) return false;
  bool main = Same(state.main, entry->key);
  // Observation only: reported after the receiver is validated, so an invalid
  // receiver still throws from the same place and records nothing.
  state.Observe("Document", FieldName(field), int(ApiAccess::Read));
  if (field == Location || field == View) {
    args.rval().setObjectOrNull(main ? field == Location ? state.environment.location() : state.page.get() : nullptr);
    return true;
  }
  DocumentSnapshot value;
  if (!state.Check(state.model.InspectDocument(entry->key, &value))) return false;
  if (field == URL) {
    if (!value.url) return state.environment.URL(args.rval());
    return Value(cx, value.url, args.rval());
  }
  if (field == Root) return state.Wrap(value.document_element, args.rval());
  if (field == Head) return state.Wrap(value.head, args.rval());
  if (field == Body) return state.Wrap(value.body, args.rval());
  if (field == Active) return state.Wrap(value.active_element, args.rval());
  if (field == Script) return state.Wrap(value.current_script, args.rval());
  if (field == Hidden) {
    args.rval().setBoolean(value.hidden);
    return true;
  }
  if (field == Visibility) return Value(cx, DomStringView(value.hidden ? u"hidden" : u"visible"), args.rval());
  if (field == Encoding) return Value(cx, DomStringView(u"UTF-8"), args.rval());
  if (field == ContentType)
    return Value(cx, DomStringView(value.content_type), args.rval());
  return Value(cx, DomStringView(value.ready_state == ReadyState::Loading ? u"loading" :
                  value.ready_state == ReadyState::Interactive ? u"interactive" : u"complete"), args.rval());
}
// document.title: the tree's head title element is the only source, so the
// setter rewrites its text and never keeps a parallel string on the wrapper.
static bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = NodeReceiver(cx, args.thisv(), NodeKind::Document);
  if (!entry) return false;
  State& state = *entry->owner;
  state.Observe("Document", "title", int(ApiAccess::Write));
  DomString value;
  if (!String(cx, args.get(0), &value)) return false;
  {
    JSAutoRealm realm(cx, state.page);
    if (!state.Check(state.model.SetTitle(entry->key, value))) return false;
  }
  args.rval().setUndefined();
  return true;
}
// document.location's setter is the same navigation the Location object's own
// href accessor performs. The Document owner keeps no second navigation path
// and no navigation state: it forwards to the Location instance the
// environment owns, so one owner still commits the URL and the history entry.
static bool SetLocation(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  NodeKind kind = NodeKind::Document;
  Wrapper* entry = Receiver(state, args, &kind);
  if (!entry) return false;
  state.Observe("Document", "location", int(ApiAccess::Write));
  // Only the main document has a browsing context and therefore a Location.
  JSObject* location =
      Same(state.main, entry->key) ? state.environment.location() : nullptr;
  if (!location) return TypeError(cx, "location is not available for this document");
  JS::RootedObject target(cx, location);
  JS::RootedValue value(cx, args.get(0));
  if (!JS_SetProperty(cx, target, "href", value)) return false;
  args.rval().setUndefined();
  return true;
}
static bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  NodeKind kind = NodeKind::Document;
  Wrapper* entry = Receiver(state, args, &kind);
  if (!entry) return false;
  auto op = Method(FunctionSlot(args));
  if (op == MakeRange) return CreateRange(state, entry->key, args.rval());
  if (op == TreeWalker || op == NodeIterator) {
    if (!argc) return TypeError(cx, "Document method has too few arguments");
    return op == TreeWalker
        ? CreateTreeWalker(state, args.get(0), args.get(1), args.get(2), args.rval())
        : CreateNodeIterator(state, args.get(0), args.get(1), args.get(2), args.rval());
  }
  if (op == AdoptNode) {
    if (!argc) return TypeError(cx, "Document method has too few arguments");
    Wrapper* source = NodeReceiver(cx, args[0]);
    if (!source) return false;
    if (source->owner != entry->owner) return AdoptForeign(*entry, *source, args.rval());
    NodeKey node;
    if (!NodeArgument(state, args[0], &node)) return false;
    Draft draft;
    std::vector<std::pair<NodeKey, NodeKey>> rekey;
    const Status status = state.model.AdoptNode(entry->key, node, &draft, &rekey);
    state.Rekey(rekey);
    if (!state.Check(status)) return false;
    return state.Wrap(draft.root, args.rval());
  }
  if (op == Fragment) {
    // A DocumentFragment is an insertion container: it is created empty and
    // owns no tree position until its children are pre-inserted elsewhere.
    Draft draft;
    return state.Check(state.model.CreateFragment(entry->key, &draft)) &&
        state.Commit(draft, args.rval());
  }
  if (!argc) return TypeError(cx, "Document method has too few arguments");
  DomString value;
  if (!String(cx, args[0], &value)) return false;
  if (op == Attribute) {
    DocumentSnapshot snapshot;
    if (!state.Check(state.model.InspectDocument(entry->key, &snapshot))) return false;
    DomString name = value;
    if (!AttributeName(snapshot.mode == DocumentMode::Html, &name))
      return ThrowDOM(state, "InvalidCharacterError", "Invalid attribute name");
    return DetachedAttribute(state, name, args.rval());
  }
  if (op == Id) {
    std::vector<NodeKey> found;
    if (!state.Check(state.model.Query(entry->key, QueryKind::Id, value, &found))) return false;
    return state.Wrap(found.empty() ? NodeKey{} : found.front(), args.rval());
  }
  if (op == Element && !args.get(1).isNullOrUndefined()) {
    // Customized built-ins need a CustomElement owner; never silently construct one.
    if (args[1].isObject()) {
      JS::RootedObject options(cx, &args[1].toObject());
      JS::RootedValue custom(cx);
      if (!JS_GetProperty(cx, options, "is", &custom)) return false;
      if (!custom.isUndefined()) {
        DomString converted;
        if (!String(cx, custom, &converted)) return false;
        return ThrowDOM(state, "NotSupportedError", "Customized built-in elements are not implemented");
      }
    } else {
      DomString converted;
      if (!String(cx, args[1], &converted)) return false;
      return ThrowDOM(state, "NotSupportedError", "Legacy custom element options are not implemented");
    }
  }
  if (op == Element && Same(entry->key, state.main)) {
    // A defined autonomous custom element is constructed synchronously (HTML
    // "create an element", synchronous custom elements flag set).
    DomString lowered = value;
    for (char16_t& c : lowered) if (c >= u'A' && c <= u'Z') c = char16_t(c + 32);
    if (CustomElementDefinition* definition = FindDefinition(state, DomStringView(lowered)))
      return CreateDefinedElement(state, *definition, args.rval());
  }
  CreateSpec spec{op == Element ? NodeKind::Element : op == Text ? NodeKind::Text : NodeKind::Comment,
                  op == Element ? DomStringView(value) : DomStringView(),
                  op == Element ? DomStringView() : DomStringView(value)};
  Draft draft;
  return state.Check(state.model.Create(entry->key, spec, &draft)) && state.Commit(draft, args.rval());
}
bool InstallDocument(State& state) {
  if (!InstallDocumentCreateEvent(state) || !InstallDocumentStream(state) || !InstallDocumentMetadata(state) ||
      !InstallDocumentCommands(state) || !InstallFragmentDirective(state)) return false;
  struct Property { const char* name; Field field; bool writable; };
  static const Property properties[] = {
    {"documentElement", Root, false}, {"head", Head, false}, {"body", Body, false},
    {"readyState", Ready, false}, {"URL", URL, false}, {"documentURI", URL, false},
    {"defaultView", View, false}, {"activeElement", Active, false},
    {"currentScript", Script, false}, {"hidden", Hidden, false},
    {"visibilityState", Visibility, false}, {"contentType", ContentType, false},
    {"characterSet", Encoding, false}, {"charset", Encoding, false},
    {"inputEncoding", Encoding, false}, {"title", Title, true},
    {"doctype", Doctype, false}
  };
  for (const auto& property : properties)
    if (!state.Accessor("Document", property.name, property.field == Doctype ? ParsedDocumentTypeGetter : Get, property.field,
                        property.writable ? Set : nullptr)) return false;
  struct Operation { const char* name; Method method; };
  static const Operation methods[] = {
    {"createElement", Element}, {"createTextNode", Text}, {"createComment", Comment}, {"getElementById", Id},
    {"createDocumentFragment", Fragment}, {"createAttribute", Attribute},
    {"importNode", ImportNode}, {"adoptNode", AdoptNode},
    {"createTreeWalker", TreeWalker}, {"createNodeIterator", NodeIterator},
    {"createRange", MakeRange}
  };
  for (const auto& method : methods)
    if (!state.Method("Document", method.name, method.method == ImportNode ? DocumentImport : Call,
        method.method == Fragment || method.method == MakeRange ? 0 : 1, method.method)) return false;
  if (!state.Method("Document", "createElementNS", CreateElementNS, 2, 0)) return false;
  state.documentLocationGetter = state.Function(Get, "get location", 0, Location);
  // D3: the donor's own accessor has both get and set, so the setter that
  // forwards to the Location owner is the one production installer.
  state.documentLocationSetter = state.Function(SetLocation, "set location", 1, Location);
  return state.documentLocationGetter && state.documentLocationSetter &&
      state.helpers.Member(state.documentLocationGetter, "Document", "get location", "get") &&
      state.helpers.Member(state.documentLocationSetter, "Document", "set location", "set") &&
      InstallXPath(state) && InstallSelection(state) && InstallDocumentView(state) && InstallDocumentNodes(state) &&
      InstallImplementationObject(state);
}
}
