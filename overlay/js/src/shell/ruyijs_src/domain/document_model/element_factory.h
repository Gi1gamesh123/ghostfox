#pragma once
#include "domain/document_model/node_type.h"
#include "domain/document_model/foreign_element_interfaces.h"
#include <algorithm>

namespace ruyijs::domain::document_model {
inline const char* foreign_element_interface(const TagName& tag) {
  if (tag.namespace16() == u"http://www.w3.org/2000/svg") {
    for (const auto& row : kSvgElementInterfaces)
      if (*row.tag && tag.local16() == row.tag) return row.name;
    return "SVGElement";
  }
  return tag.namespace16() == u"http://www.w3.org/1998/Math/MathML" ? "MathMLElement" : "Element";
}
class ElementFactory {
 public:
  static const std::vector<std::string>& known_elements() {
    static const std::vector<std::string> names = {
      "HTML","HEAD","BODY","TITLE","BASE","LINK","META","STYLE","SCRIPT","NOSCRIPT","TEMPLATE","SLOT",
      "DIV","SPAN","P","A","IMG","BR","HR","H1","H2","H3","H4","H5","H6","UL","OL","LI","DL","DT","DD",
      "TABLE","THEAD","TBODY","TFOOT","TR","TD","TH","CAPTION","COLGROUP","COL","FORM","INPUT","BUTTON",
      "SELECT","OPTION","OPTGROUP","TEXTAREA","LABEL","FIELDSET","LEGEND","DATALIST","OUTPUT","PROGRESS","METER",
      "SECTION","ARTICLE","ASIDE","HEADER","FOOTER","NAV","MAIN","FIGURE","FIGCAPTION","VIDEO","AUDIO","SOURCE",
      "TRACK","CANVAS","IFRAME","EMBED","OBJECT","PARAM","PICTURE","MAP","AREA","DETAILS","SUMMARY","DIALOG","MENU",
      "PRE","BLOCKQUOTE","CODE","EM","STRONG","SMALL","MARK","ABBR","TIME","DATA","VAR","SAMP","KBD","SUB","SUP",
      "I","B","U","S","WBR","BDO","BDI","RUBY","RT","RP",
      "ACRONYM","ADDRESS","BASEFONT","BIG","CENTER","CITE","DEL","DFN","DIR","FONT","FRAME","FRAMESET",
      "HGROUP","IMAGE","INS","LISTING","MARQUEE","NOBR","NOEMBED","NOFRAMES","PLAINTEXT","Q","RB","RTC","SEARCH","STRIKE","TT","XMP"
    };
    return names;
  }
  static bool is_known_element(const TagName& tag) {
    if (!tag.html()) return false;
    const auto& value = tag.local16();
    for (const auto& name : known_elements()) {
      if (name.size() != value.size()) continue;
      bool equal = true;
      for (size_t i = 0; i < name.size(); ++i)
        if (value[i] != char16_t(name[i] >= 'A' && name[i] <= 'Z' ? name[i] + 32 : name[i])) { equal = false; break; }
      if (equal) return true;
    }
    return false;
  }
  static bool is_custom_name(DomStringView name) {
    if (name.empty() || name[0] < u'a' || name[0] > u'z' || name.find(u'-') == DomStringView::npos) return false;
    const char16_t* reserved[] = {u"annotation-xml",u"color-profile",u"font-face",u"font-face-src",u"font-face-uri",u"font-face-format",u"font-face-name",u"missing-glyph"};
    for (auto* value : reserved) if (name == value) return false;
    for (char16_t c : name) if (c == u':' || (c >= u'A' && c <= u'Z')) return false;
    return valid_name(name);
  }
  static const char* interface_name_static(const TagName& tag) {
    if (!tag.html()) return foreign_element_interface(tag);
    const auto& t = tag.local16();
    struct Row { const char16_t* tag; const char* interface; };
    static constexpr Row rows[] = {
      {u"div","HTMLDivElement"},{u"span","HTMLSpanElement"},{u"a","HTMLAnchorElement"},
      {u"img","HTMLImageElement"},{u"input","HTMLInputElement"},{u"form","HTMLFormElement"},
      {u"script","HTMLScriptElement"},{u"canvas","HTMLCanvasElement"},{u"iframe","HTMLIFrameElement"},
      {u"video","HTMLVideoElement"},{u"audio","HTMLAudioElement"},{u"button","HTMLButtonElement"},
      {u"select","HTMLSelectElement"},{u"textarea","HTMLTextAreaElement"},{u"table","HTMLTableElement"},
      {u"body","HTMLBodyElement"},{u"head","HTMLHeadElement"},{u"html","HTMLHtmlElement"},
      {u"p","HTMLParagraphElement"},{u"br","HTMLBRElement"},{u"hr","HTMLHRElement"},
      {u"ul","HTMLUListElement"},{u"ol","HTMLOListElement"},{u"li","HTMLLIElement"},
      {u"dl","HTMLDListElement"},{u"title","HTMLTitleElement"},{u"meta","HTMLMetaElement"},
      {u"link","HTMLLinkElement"},{u"base","HTMLBaseElement"},{u"style","HTMLStyleElement"},
      {u"option","HTMLOptionElement"},{u"optgroup","HTMLOptGroupElement"},{u"label","HTMLLabelElement"},
      {u"fieldset","HTMLFieldSetElement"},{u"legend","HTMLLegendElement"},{u"datalist","HTMLDataListElement"},
      {u"output","HTMLOutputElement"},{u"progress","HTMLProgressElement"},{u"meter","HTMLMeterElement"},
      {u"source","HTMLSourceElement"},{u"track","HTMLTrackElement"},{u"embed","HTMLEmbedElement"},
      {u"object","HTMLObjectElement"},{u"param","HTMLParamElement"},{u"picture","HTMLPictureElement"},
      {u"map","HTMLMapElement"},{u"area","HTMLAreaElement"},{u"details","HTMLDetailsElement"},
      {u"dialog","HTMLDialogElement"},{u"menu","HTMLMenuElement"},{u"pre","HTMLPreElement"},
      {u"blockquote","HTMLQuoteElement"},{u"time","HTMLTimeElement"},{u"data","HTMLDataElement"},
      {u"caption","HTMLTableCaptionElement"},{u"col","HTMLTableColElement"},{u"colgroup","HTMLTableColElement"},
      {u"thead","HTMLTableSectionElement"},{u"tbody","HTMLTableSectionElement"},{u"tfoot","HTMLTableSectionElement"},
      {u"tr","HTMLTableRowElement"},{u"td","HTMLTableCellElement"},{u"th","HTMLTableCellElement"},
      {u"h1","HTMLHeadingElement"},{u"h2","HTMLHeadingElement"},{u"h3","HTMLHeadingElement"},
      {u"h4","HTMLHeadingElement"},{u"h5","HTMLHeadingElement"},{u"h6","HTMLHeadingElement"},
      {u"template","HTMLTemplateElement"},{u"slot","HTMLSlotElement"},
      {u"q","HTMLQuoteElement"},{u"ins","HTMLModElement"},{u"del","HTMLModElement"},
      {u"dir","HTMLDirectoryElement"},{u"font","HTMLFontElement"},{u"frame","HTMLFrameElement"},
      {u"frameset","HTMLFrameSetElement"},{u"marquee","HTMLMarqueeElement"},
      {u"listing","HTMLPreElement"},{u"xmp","HTMLPreElement"}
    };
    for (const auto& row : rows) if (t == row.tag) return row.interface;
    return is_known_element(tag) || (!tag.generic_unknown() && is_custom_name(t)) ? "HTMLElement"
        : tag.generic_unknown() ? "Element" : "HTMLUnknownElement";
  }
  static std::string interface_name(const TagName& tag) { return interface_name_static(tag); }
};
}
