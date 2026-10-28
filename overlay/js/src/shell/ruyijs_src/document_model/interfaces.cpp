#include "document_model/state.h"
#include "document_model/parsing.h"
#include "document_model/sanitizer.h"
#include "document_model/range.h"
#include "document_model/xpath.h"
#include "interaction/realm.h"
#include "runtime/native_constructor_trace.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
namespace ruyijs::document_model {
bool InstallInterfaces(State& state) {
  JS::RootedObject parent(state.cx, state.interaction.prototype());
  if (!state.DefineInterface("Node", parent, Illegal)) return false;
  struct Spec { const char* name; const char* parent; ConstructorKind kind; };
  static const Spec specs[] = {
    {"Element", "Node", Illegal}, {"HTMLElement", "Element", NewCustomElement},
    {"CharacterData", "Node", Illegal}, {"Text", "CharacterData", NewText},
    {"ProcessingInstruction", "CharacterData", Illegal},
    {"Comment", "CharacterData", NewComment}, {"Document", "Node", NewDocument},
    {"DocumentFragment", "Node", NewFragment},
    {"HTMLDocument", "Document", Illegal},
    {"HTMLDivElement", "HTMLElement", NewDivElement}, {"HTMLSpanElement", "HTMLElement", Illegal},
    {"HTMLAnchorElement", "HTMLElement", Illegal}, {"HTMLImageElement", "HTMLElement", Illegal},
    {"HTMLInputElement", "HTMLElement", NewInputElement}, {"HTMLFormElement", "HTMLElement", Illegal},
    {"HTMLScriptElement", "HTMLElement", NewScriptElement}, {"HTMLCanvasElement", "HTMLElement", Illegal},
    {"HTMLLinkElement", "HTMLElement", NewLinkElement}, {"HTMLBaseElement", "HTMLElement", NewBaseElement},
    {"HTMLAreaElement", "HTMLElement", NewAreaElement},
    {"HTMLDetailsElement", "HTMLElement", NewDetailsElement},
    {"HTMLDialogElement", "HTMLElement", NewDialogElement},
    {"HTMLTemplateElement", "HTMLElement", NewTemplateElement},
    {"HTMLSourceElement", "HTMLElement", NewSourceElement},
    {"HTMLMarqueeElement", "HTMLElement", NewMarqueeElement},
    {"HTMLIFrameElement", "HTMLElement", NewIFrameElement}, {"HTMLMediaElement", "HTMLElement", Illegal},
    {"HTMLVideoElement", "HTMLMediaElement", Illegal}, {"HTMLAudioElement", "HTMLMediaElement", Illegal},
    {"HTMLButtonElement", "HTMLElement", NewButtonElement}, {"HTMLSelectElement", "HTMLElement", Illegal},
    {"HTMLOptionElement", "HTMLElement", NewOptionElement},
    {"HTMLOptGroupElement", "HTMLElement", NewOptGroupElement},
    {"HTMLLabelElement", "HTMLElement", NewLabelElement},
    {"HTMLDataListElement", "HTMLElement", NewDataListElement},
    {"HTMLMeterElement", "HTMLElement", NewMeterElement},
    {"HTMLProgressElement", "HTMLElement", NewProgressElement},
    {"HTMLTextAreaElement", "HTMLElement", NewTextAreaElement}, {"HTMLTableElement", "HTMLElement", Illegal},
    {"HTMLBodyElement", "HTMLElement", Illegal}, {"HTMLHeadElement", "HTMLElement", Illegal},
    {"HTMLHtmlElement", "HTMLElement", NewHtmlElement}, {"HTMLUnknownElement", "HTMLElement", Illegal},
    {"HTMLStyleElement", "HTMLElement", NewStyleElement}, {"HTMLBRElement", "HTMLElement", NewBRElement},
    {"HTMLParagraphElement", "HTMLElement", NewParagraphElement}, {"HTMLHeadingElement", "HTMLElement", NewHeadingElement},
    {"HTMLDataElement", "HTMLElement", NewDataElement}, {"HTMLDListElement", "HTMLElement", NewDListElement},
    {"HTMLDirectoryElement", "HTMLElement", NewDirectoryElement}, {"HTMLOListElement", "HTMLElement", NewOListElement},
    {"HTMLLIElement", "HTMLElement", NewLIElement}
  };
  for (const auto& spec : specs) {
    parent = state.Find(spec.parent)->prototype;
    if (!state.DefineInterface(spec.name, parent, spec.kind)) return false;
  }
  if (!InstallForeignElementInterfaces(state)) return false;
  parent = state.Find("DocumentFragment")->prototype;
  if (!state.DefineInterface("ShadowRoot", parent, Illegal, false)) return false;
  parent = state.Find("Text")->prototype;
  if (!state.DefineInterface("CDATASection", parent, Illegal, false)) return false;
  parent = state.Find("Node")->prototype;
  if (!state.DefineInterface("DocumentType", parent, Illegal, false)) return false;
  parent = state.Find("Document")->prototype;
  if (!state.DefineInterface("XMLDocument", parent, Illegal, false)) return false;
  static const char* html[] = {
    "HTMLHRElement", "HTMLUListElement", "HTMLTitleElement", "HTMLMetaElement",
    "HTMLFieldSetElement", "HTMLLegendElement", "HTMLOutputElement",
    "HTMLTrackElement", "HTMLEmbedElement",
    "HTMLObjectElement", "HTMLParamElement", "HTMLPictureElement", "HTMLMapElement",
    "HTMLMenuElement",
    "HTMLPreElement", "HTMLQuoteElement", "HTMLTimeElement",
    "HTMLTableCaptionElement", "HTMLTableColElement", "HTMLTableSectionElement",
    "HTMLTableRowElement", "HTMLTableCellElement",
    "HTMLSlotElement", "HTMLModElement",
    "HTMLFontElement", "HTMLFrameElement", "HTMLFrameSetElement"
  };
  parent = state.Find("HTMLElement")->prototype;
  for (const char* name : html)
    if (!state.DefineInterface(name, parent, Illegal)) return false;
  // Collection types chain to Object.prototype, matching the reference surface.
  JS::RootedObject plain(state.cx, JS::GetRealmObjectPrototype(state.cx));
  static const char* collections[] = {"NodeList", "HTMLCollection", "DOMTokenList",
                                      "NamedNodeMap", "Attr", "CSSStyleDeclaration"};
  for (const char* name : collections)
    if (!state.DefineInterface(name, plain, Illegal)) return false;
  if (!state.DefineInterface("CustomElementRegistry", plain, Illegal, false)) return false;
  parent = state.Find("CSSStyleDeclaration")->prototype;
  if (!state.DefineInterface("CSSStyleProperties", parent, Illegal)) return false;
  if (!state.DefineInterface("HTMLAllCollection", plain, Illegal, false)) return false;
  parent = state.Find("HTMLCollection")->prototype;
  if (!state.DefineInterface("HTMLOptionsCollection", parent, Illegal) ||
      !state.DefineInterface("HTMLFormControlsCollection", parent, Illegal) ||
      !state.DefineInterface("ValidityState", plain, Illegal)) return false;
  parent = state.Find("NodeList")->prototype;
  if (!state.DefineInterface("RadioNodeList", parent, Illegal)) return false;
  // Traversal interfaces chain to Object.prototype like the collections.
  static const char* traversal[] = {"NodeFilter", "TreeWalker", "NodeIterator"};
  for (const char* name : traversal)
    if (!state.DefineInterface(name, plain, Illegal)) return false;
  if (!state.DefineInterface("AbstractRange", plain, Illegal)) return false;
  parent = state.Find("AbstractRange")->prototype;
  if (!state.DefineInterface("Range", parent, NewRangeObject) ||
      !state.DefineInterface("StaticRange", parent, NewStaticRange)) return false;
  // Parsing/serialization services are constructible platform interfaces.
  if (!state.DefineInterface("DOMParser", plain, NewDOMParser, false)) return false;
  if (!state.DefineInterface("XMLSerializer", plain, NewXMLSerializer, false)) return false;
  if (!state.DefineInterface("DOMImplementation", plain, NewDOMImplementation)) return false;
  if (!state.DefineInterface("XPathEvaluator", plain, NewXPathEvaluator) ||
      !state.DefineInterface("XPathExpression", plain, Illegal) ||
      !state.DefineInterface("XPathResult", plain, Illegal) ||
      !state.DefineInterface("Selection", plain, Illegal)) return false;
  return true;
}
bool Constructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  State& state = *FunctionState(args);
  int32_t kind = FunctionSlot(args);
  if (kind == NewCustomElement) return trace.Finish(ConstructCustomElement(state, args));
  if (kind == NewXslt && !args.isConstructing()) return TypeError(cx, "XSLTProcessor constructor: 'new' is required");
  if (kind == NewSanitizerKind && !args.isConstructing()) return TypeError(cx, "Sanitizer constructor: 'new' is required");
  if (kind == NewSanitizerKind) return trace.Finish(NewSanitizer(state, args));
  if (!args.isConstructing() || kind == Illegal) return TypeError(cx, "Illegal constructor.");
  if (kind == NewXslt) return trace.Finish(NewXsltProcessor(state, args));
  if (kind == NewXPathEvaluator)
    return trace.Finish(NewXPath(state, XPathKind::Evaluator, 0, args.rval(), &args));
  if (kind == NewRangeObject || kind == NewStaticRange) {
    bool ok = kind == NewRangeObject ? CreateRange(state, state.main, args.rval(), &args)
                                    : CreateStaticRange(state, args);
    return trace.Finish(ok);
  }
  if (kind == NewDOMParser || kind == NewXMLSerializer || kind == NewDOMImplementation) {
    int service = kind == NewDOMParser ? 0 : kind == NewXMLSerializer ? 1 : 2;
    if (!NewService(state, service, args.rval(), service != 2 ? &args : nullptr)) return false;
    return trace.Finish(true);
  }
  Draft draft;
  if (kind == NewDocument) {
    if (!state.Check(state.model.CreateDocument(DocumentInit::EmptyXml, &draft))) return false;
  } else if (kind == NewFragment) {
    if (!state.Check(state.model.CreateFragment(state.main, &draft))) return false;
  } else if (kind == NewOptionElement || kind == NewOptGroupElement || kind == NewButtonElement ||
             kind == NewInputElement || kind == NewTextAreaElement || kind == NewLabelElement ||
             kind == NewDataListElement || kind == NewMeterElement || kind == NewProgressElement ||
             kind == NewLinkElement || kind == NewBaseElement || kind == NewScriptElement || kind == NewAreaElement ||
             kind == NewDetailsElement || kind == NewDialogElement ||
             kind == NewTemplateElement ||
             kind == NewSourceElement || kind == NewMarqueeElement || kind == NewIFrameElement ||
             (kind >= NewStyleElement && kind <= NewHtmlElement)) {
    static const char16_t* contentTags[] = {u"style",u"br",u"div",u"p",u"h1",u"data",u"dl",u"dir",u"ol",u"li",u"html"};
    static_assert(std::size(contentTags) == NewHtmlElement - NewStyleElement + 1);
    CreateSpec spec{NodeKind::Element, kind >= NewStyleElement ? contentTags[kind-NewStyleElement] :
        kind == NewOptionElement ? u"option" :
        kind == NewOptGroupElement ? u"optgroup" : kind == NewInputElement ? u"input" :
        kind == NewTextAreaElement ? u"textarea" : kind == NewLabelElement ? u"label" :
        kind == NewDataListElement ? u"datalist" : kind == NewMeterElement ? u"meter" :
        kind == NewProgressElement ? u"progress" : kind == NewLinkElement ? u"link" :
        kind == NewBaseElement ? u"base" : kind == NewScriptElement ? u"script" :
        kind == NewAreaElement ? u"area" : kind == NewDetailsElement ? u"details" :
        kind == NewDialogElement ? u"dialog" : kind == NewTemplateElement ? u"template" :
        kind == NewSourceElement ? u"source" : kind == NewMarqueeElement ? u"marquee" :
        kind == NewIFrameElement ? u"iframe" : u"button", {}};
    if (!state.Check(state.model.Create(state.main, spec, &draft))) return false;
  } else {
    DomString data;
    if (!args.get(0).isUndefined() && !String(cx, args[0], &data)) return false;
    CreateSpec spec{kind == NewText ? NodeKind::Text : NodeKind::Comment, {}, data};
    if (!state.Check(state.model.Create(state.main, spec, &draft))) return false;
  }
  if (!state.Commit(draft, args.rval(), &args)) return false;
  return trace.Finish(true);
}
}
