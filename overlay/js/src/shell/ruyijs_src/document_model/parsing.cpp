#include "document_model/parsing.h"

#include "document_model/state.h"
#include "browser_environment/environment.h"
#include "js/Object.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
// Public parsing services accept original branded instances across realms.
bool Service(State& state, const JS::CallArgs& args, int service) {
  JSObject* object = args.thisv().isObject() ? &args.thisv().toObject() : nullptr;
  if (object) object = js::CheckedUnwrapStatic(object);
  if (!object || JS::GetClass(object) != &kServiceClass)
    return TypeError(state.cx, "Illegal invocation");
  if (int(JS::GetReservedSlot(object, 1).toInt32()) != service)
    return TypeError(state.cx, "Illegal invocation");
  return true;
}

// parseFromString: text/html through the HTML parser, the four XML media
// types through the XML parser (model_dom_parser.cpp); the MIME type is kept.
bool ParseFromString(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  if (!Service(state, args, 0)) return false;
  if (argc < 2) return TypeError(cx, "parseFromString requires two arguments");
  state.Observe("DOMParser", "parseFromString", int(ApiAccess::Call));
  DomString markup, type;
  if (!PublicDocumentString(cx, args[0], &markup) || !PublicDocumentString(cx, args[1], &type)) return false;
  ParseKind kind;
  if (type == u"text/html") kind = ParseKind::Html;
  else if (type == u"application/xml") kind = ParseKind::Xml;
  else if (type == u"text/xml") kind = ParseKind::TextXml;
  else if (type == u"image/svg+xml") kind = ParseKind::Svg;
  else if (type == u"application/xhtml+xml") kind = ParseKind::Xhtml;
  else return TypeError(cx, "Unsupported type for DOMParser");
  // A parsererror document names the creating document's URL (Firefox).
  DocumentSnapshot creator;
  DomString location;
  if (state.model.InspectDocument(state.main, &creator).code == ErrorCode::None && creator.url) {
    location = *creator.url;
  } else {  // The page's URL is the environment's, as document.URL reads it.
    JS::RootedValue url(cx);
    if (!state.environment.URL(&url) || (url.isString() && !String(cx, url, &location))) return false;
  }
  Draft draft;
  return state.Check(state.model.ParseDocument(kind, markup, &draft, location)) &&
      state.Commit(draft, args.rval());
}

// serializeToString(node): the existing DomTreeService outer serialization.
bool SerializeToString(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  if (!Service(state, args, 1)) return false;
  if (!argc) return TypeError(cx, "serializeToString requires one argument");
  Wrapper* node = NodeReceiver(cx, args[0]);
  if (!node) return false;
  state.Observe("XMLSerializer", "serializeToString", int(ApiAccess::Call));
  State& owner = *node->owner;
  DomString text;
  return owner.Check(owner.model.OuterHtml(node->key, &text)) &&
      Value(cx, DomStringView(text), args.rval());
}
// The public static entry accepts a string without consulting user conversion
// or options getters. It shares the original inert parser and document owner.
bool ParseHTMLUnsafe(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  if (!argc) return TypeError(cx, "parseHTMLUnsafe requires one argument");
  if (!args[0].isString()) return TypeError(cx, "parseHTMLUnsafe requires a string");
  state.Observe("Document", "parseHTMLUnsafe", int(ApiAccess::Call));
  DomString markup;
  if (!String(cx, args[0], &markup)) return false;
  Draft draft;
  return state.Check(state.model.ParseDocument(ParseKind::PublicHtml, markup, &draft)) &&
      state.Commit(draft, args.rval());
}
// createHTMLDocument(title): a fresh HTML document built by the same parser.
bool CreateHTMLDocument(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  if (!Service(state, args, 2)) return false;
  DomString title;
  if (!args.get(0).isUndefined() && !String(cx, args.get(0), &title)) return false;
  DomString markup = u"<!doctype html><title>";
  for (char16_t unit : title) {
    if (unit == u'&') markup += u"&amp;";
    else if (unit == u'<') markup += u"&lt;";
    else if (unit == u'>') markup += u"&gt;";
    else markup += unit;
  }
  markup += u"</title>";
  Draft draft;
  return state.Check(state.model.ParseDocument(ParseKind::Html, markup, &draft)) &&
      state.Commit(draft, args.rval());
}

bool HasFeature(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  if (!Service(state, args, 2)) return false;
  args.rval().setBoolean(true);
  return true;
}

}  // namespace

bool InstallParsing(State& state) {
  JS::RootedObject target(state.cx, state.Find("Document")->constructor);
  JS::RootedObject parse(state.cx, state.Function(ParseHTMLUnsafe, "parseHTMLUnsafe", 1, 0));
  return parse && JS_DefineProperty(state.cx, target, "parseHTMLUnsafe", parse, JSPROP_ENUMERATE) &&
         state.Method("DOMParser", "parseFromString", ParseFromString, 2, 0) &&
         state.Method("XMLSerializer", "serializeToString", SerializeToString, 1, 0) &&
         state.Method("DOMImplementation", "createHTMLDocument", CreateHTMLDocument, 1, 0) &&
         state.Method("DOMImplementation", "hasFeature", HasFeature, 0, 0) &&
         state.Method("DOMImplementation", "createDocument", CreateXMLDocument, 2, 0) &&
         InstallDocumentType(state);
}
}  // namespace ruyijs::document_model
