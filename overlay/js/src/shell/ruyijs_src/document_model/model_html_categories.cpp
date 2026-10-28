// HTML element categories used by tree construction (WHATWG "special",
// scope boundaries and implied end tags), restricted to the HTML namespace.
#include "document_model/model_html.h"

namespace ruyijs::document_model::html {
bool In(DomStringView name, std::initializer_list<const char16_t*> names) {
  for (auto* entry : names) if (name == entry) return true;
  return false;
}
bool HeadVoid(DomStringView n) { return In(n, {u"base", u"basefont", u"bgsound", u"link", u"meta"}); }
bool HeadContent(DomStringView n) {
  return HeadVoid(n) || In(n, {u"noframes", u"script", u"style", u"template", u"title", u"noscript"});
}
bool ClosesP(DomStringView n) {
  return In(n, {u"address", u"article", u"aside", u"blockquote", u"center", u"details", u"dialog",
                u"dir", u"div", u"dl", u"fieldset", u"figcaption", u"figure", u"footer", u"form",
                u"header", u"hgroup", u"hr", u"main", u"menu", u"nav", u"ol", u"p", u"pre",
                u"listing", u"plaintext", u"search", u"section", u"summary", u"table", u"ul",
                u"xmp", u"h1", u"h2", u"h3", u"h4", u"h5", u"h6"});
}
bool Heading(DomStringView n) { return In(n, {u"h1", u"h2", u"h3", u"h4", u"h5", u"h6"}); }
bool Implied(DomStringView n) {
  return In(n, {u"dd", u"dt", u"li", u"optgroup", u"option", u"p", u"rb", u"rp", u"rt", u"rtc"});
}
bool Boundary(DomStringView n) {
  return In(n, {u"applet", u"caption", u"html", u"table", u"td", u"th", u"marquee", u"object", u"template"});
}
bool Special(DomStringView n) {
  return Boundary(n) || HeadContent(n) || ClosesP(n) ||
         In(n, {u"area", u"body", u"br", u"button", u"col", u"colgroup", u"dd", u"dt", u"embed",
                u"frame", u"frameset", u"head", u"iframe", u"img", u"input", u"li", u"noembed",
                u"param", u"select", u"source", u"tbody", u"textarea", u"tfoot", u"thead", u"tr",
                u"track", u"wbr"});
}
}
