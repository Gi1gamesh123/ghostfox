// HTML parser foreign content tables (WHATWG "parsing main inforeign" and the
// adjust SVG/MathML/foreign attributes steps). Tree construction itself stays
// in model_tree_builder.cpp; this file only names and maps.
#include "document_model/model_internal.h"
#include <algorithm>
#include <utility>

namespace ruyijs::document_model::html {
namespace {
using Pair = std::pair<const char16_t*, const char16_t*>;
const Pair kSvgTags[] = {
  {u"altglyph", u"altGlyph"}, {u"altglyphdef", u"altGlyphDef"}, {u"altglyphitem", u"altGlyphItem"},
  {u"animatecolor", u"animateColor"}, {u"animatemotion", u"animateMotion"},
  {u"animatetransform", u"animateTransform"}, {u"clippath", u"clipPath"}, {u"feblend", u"feBlend"},
  {u"fecolormatrix", u"feColorMatrix"}, {u"fecomponenttransfer", u"feComponentTransfer"},
  {u"fecomposite", u"feComposite"}, {u"feconvolvematrix", u"feConvolveMatrix"},
  {u"fediffuselighting", u"feDiffuseLighting"}, {u"fedisplacementmap", u"feDisplacementMap"},
  {u"fedistantlight", u"feDistantLight"}, {u"fedropshadow", u"feDropShadow"}, {u"feflood", u"feFlood"},
  {u"fefunca", u"feFuncA"}, {u"fefuncb", u"feFuncB"}, {u"fefuncg", u"feFuncG"}, {u"fefuncr", u"feFuncR"},
  {u"fegaussianblur", u"feGaussianBlur"}, {u"feimage", u"feImage"}, {u"femerge", u"feMerge"},
  {u"femergenode", u"feMergeNode"}, {u"femorphology", u"feMorphology"}, {u"feoffset", u"feOffset"},
  {u"fepointlight", u"fePointLight"}, {u"fespecularlighting", u"feSpecularLighting"},
  {u"fespotlight", u"feSpotLight"}, {u"fetile", u"feTile"}, {u"feturbulence", u"feTurbulence"},
  {u"foreignobject", u"foreignObject"}, {u"glyphref", u"glyphRef"}, {u"lineargradient", u"linearGradient"},
  {u"radialgradient", u"radialGradient"}, {u"textpath", u"textPath"},
};
const Pair kSvgAttributes[] = {
  {u"attributename", u"attributeName"}, {u"attributetype", u"attributeType"},
  {u"basefrequency", u"baseFrequency"}, {u"baseprofile", u"baseProfile"}, {u"calcmode", u"calcMode"},
  {u"clippathunits", u"clipPathUnits"}, {u"diffuseconstant", u"diffuseConstant"}, {u"edgemode", u"edgeMode"},
  {u"filterunits", u"filterUnits"}, {u"glyphref", u"glyphRef"}, {u"gradienttransform", u"gradientTransform"},
  {u"gradientunits", u"gradientUnits"}, {u"kernelmatrix", u"kernelMatrix"},
  {u"kernelunitlength", u"kernelUnitLength"}, {u"keypoints", u"keyPoints"}, {u"keysplines", u"keySplines"},
  {u"keytimes", u"keyTimes"}, {u"lengthadjust", u"lengthAdjust"}, {u"limitingconeangle", u"limitingConeAngle"},
  {u"markerheight", u"markerHeight"}, {u"markerunits", u"markerUnits"}, {u"markerwidth", u"markerWidth"},
  {u"maskcontentunits", u"maskContentUnits"}, {u"maskunits", u"maskUnits"}, {u"numoctaves", u"numOctaves"},
  {u"pathlength", u"pathLength"}, {u"patterncontentunits", u"patternContentUnits"},
  {u"patterntransform", u"patternTransform"}, {u"patternunits", u"patternUnits"}, {u"pointsatx", u"pointsAtX"},
  {u"pointsaty", u"pointsAtY"}, {u"pointsatz", u"pointsAtZ"}, {u"preservealpha", u"preserveAlpha"},
  {u"preserveaspectratio", u"preserveAspectRatio"}, {u"primitiveunits", u"primitiveUnits"}, {u"refx", u"refX"},
  {u"refy", u"refY"}, {u"repeatcount", u"repeatCount"}, {u"repeatdur", u"repeatDur"},
  {u"requiredextensions", u"requiredExtensions"}, {u"requiredfeatures", u"requiredFeatures"},
  {u"specularconstant", u"specularConstant"}, {u"specularexponent", u"specularExponent"},
  {u"spreadmethod", u"spreadMethod"}, {u"startoffset", u"startOffset"}, {u"stddeviation", u"stdDeviation"},
  {u"stitchtiles", u"stitchTiles"}, {u"surfacescale", u"surfaceScale"}, {u"systemlanguage", u"systemLanguage"},
  {u"tablevalues", u"tableValues"}, {u"targetx", u"targetX"}, {u"targety", u"targetY"},
  {u"textlength", u"textLength"}, {u"viewbox", u"viewBox"}, {u"viewtarget", u"viewTarget"},
  {u"xchannelselector", u"xChannelSelector"}, {u"ychannelselector", u"yChannelSelector"},
  {u"zoomandpan", u"zoomAndPan"},
};
constexpr const char16_t* kXlink = u"http://www.w3.org/1999/xlink";
constexpr const char16_t* kXml = u"http://www.w3.org/XML/1998/namespace";
constexpr const char16_t* kXmlns = u"http://www.w3.org/2000/xmlns/";
template <size_t N>
const char16_t* Lookup(const Pair (&table)[N], DomStringView name) {
  for (const auto& [from, to] : table) if (name == from) return to;
  return nullptr;
}
}  // namespace

DomStringView SpaceUri(TagSpace space) noexcept {
  return space == TagSpace::Svg ? DomStringView(u"http://www.w3.org/2000/svg")
       : space == TagSpace::MathMl ? DomStringView(u"http://www.w3.org/1998/Math/MathML")
       : DomStringView(u"http://www.w3.org/1999/xhtml");
}
DomString ForeignTagName(TagSpace space, DomStringView lower) {
  const char16_t* adjusted = space == TagSpace::Svg ? Lookup(kSvgTags, lower) : nullptr;
  return adjusted ? DomString(adjusted) : DomString(lower);
}
void ForeignAttribute(TagSpace space, DomStringView lower, DomString* qualified, DomStringView* namespace_uri) {
  *namespace_uri = {};
  const char16_t* adjusted = space == TagSpace::Svg ? Lookup(kSvgAttributes, lower)
      : space == TagSpace::MathMl && lower == u"definitionurl" ? u"definitionURL" : nullptr;
  *qualified = adjusted ? DomString(adjusted) : DomString(lower);
  if (lower.substr(0, 6) == u"xlink:" &&
      In(lower.substr(6), {u"actuate", u"arcrole", u"href", u"role", u"show", u"title", u"type"}))
    *namespace_uri = kXlink;
  else if (lower == u"xml:lang" || lower == u"xml:space") *namespace_uri = kXml;
  else if (lower == u"xmlns" || lower == u"xmlns:xlink") *namespace_uri = kXmlns;
}
bool BreaksOutOfForeign(const Token& token) {
  if (token.kind == Token::Kind::EndTag) return token.name == u"br" || token.name == u"p";
  if (token.kind != Token::Kind::StartTag) return false;
  if (token.name == u"font")
    return std::any_of(token.attributes.begin(), token.attributes.end(), [](const Attribute& a) {
      return a.name == u"color" || a.name == u"face" || a.name == u"size";
    });
  return Heading(token.name) || In(token.name, {
      u"b", u"big", u"blockquote", u"body", u"br", u"center", u"code", u"dd", u"div", u"dl", u"dt", u"em",
      u"embed", u"head", u"hr", u"i", u"img", u"li", u"listing", u"menu", u"meta", u"nobr", u"ol", u"p",
      u"pre", u"ruby", u"s", u"small", u"span", u"strong", u"strike", u"sub", u"sup", u"table", u"tt", u"u",
      u"ul", u"var"});
}
bool HtmlIntegrationPoint(TagSpace space, DomStringView local) {
  return space == TagSpace::Svg ? In(local, {u"foreignObject", u"desc", u"title"})
       : space == TagSpace::MathMl && In(local, {u"mi", u"mo", u"mn", u"ms", u"mtext", u"annotation-xml"});
}
}  // namespace ruyijs::document_model::html
