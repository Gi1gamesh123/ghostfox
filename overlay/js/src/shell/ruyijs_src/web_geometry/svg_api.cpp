#include "web_geometry/svg.h"
#include "js/Realm.h"
#include <algorithm>
#include <string>

namespace ruyijs::web_geometry {
namespace {
enum Method { BBox, Count, Length, Extent, SubString };
bool Options(JSContext* cx, JS::HandleValue value, SvgOptions* result) {
  if (value.isNullOrUndefined()) return true;
  if (!value.isObject()) return WebKit::Fail(cx, "SVGGraphicsElement.getBBox: Argument 1 can't be converted to a dictionary.");
  JS::RootedObject object(cx, &value.toObject());
  const char* names[] = {"clipped", "fill", "markers", "stroke"};
  bool* fields[] = {&result->clipped, &result->fill, &result->markers, &result->stroke};
  for (unsigned i = 0; i < 4; ++i) {
    JS::RootedValue field(cx);
    if (!JS_GetProperty(cx, object, names[i], &field)) return false;
    if (!field.isUndefined()) *fields[i] = JS::ToBoolean(field);
  }
  return true;
}
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  const auto method = Method(WebKit::Slot(args));
  document_model::GeometryNode node;
  JS::RootedObject parent(cx);
  JS::RootedObjectVector children(cx);
  if (!s.svgPorts.node) return WebKit::Fail(cx, "SVG geometry source port is not installed");
  if (!s.svgPorts.node(s.svgPorts.context, args.thisv(), &node, &parent, &children)) return false;
  if (!(method == BBox ? node.graphics : node.textContent)) {
    const char* names[] = {"getBBox", "getNumberOfChars", "getComputedTextLength", "getExtentOfChar",
                           "getSubStringLength"};
    const std::string message = std::string("'") + names[method] +
        "' called on an object that does not implement interface " +
        (method == BBox ? "SVGGraphicsElement." : "SVGTextContentElement.");
    return WebKit::Fail(cx, message.c_str());
  }
  SvgOptions options;
  uint32_t index = 0, count = 0;
  if (method == BBox && !Options(cx, args.get(0), &options)) return false;
  if (method == SubString) {
    if (argc < 2) {
      const std::string message = "SVGTextContentElement.getSubStringLength: At least 2 arguments required, but only " +
          std::to_string(argc) + " passed";
      return WebKit::Fail(cx, message.c_str());
    }
    if (!JS::ToUint32(cx, args.get(0), &index) || !JS::ToUint32(cx, args.get(1), &count)) return false;
  }
  if (method == Extent) {
    if (!argc) return WebKit::Fail(cx, "SVGTextContentElement.getExtentOfChar requires one argument");
    if (!JS::ToUint32(cx, args.get(0), &index)) return false;
  }
  JS::RootedObject element(cx, &args.thisv().toObject());
  svg::SvgTextMeasurement measurement;
  bool rendered = false;
  if (!SvgMeasure(s, element, options, &measurement, &rendered)) return false;
  if (method == Count) { args.rval().setNumber(double(measurement.characters.size())); return true; }
  if (method == Length) { args.rval().setNumber(measurement.length); return true; }
  if (method == SubString) {
    // Firefox's fast path: no text frame is 0; an index past the addressable
    // characters throws; the advances of the clipped range are summed, each
    // cluster once (its characters share one box).
    if (!rendered) { args.rval().setNumber(0.0); return true; }
    if (index >= measurement.characters.size())
      return s.exception ? s.exception(s.exceptionContext, "IndexSizeError",
                                       "SVGTextContentElement.getSubStringLength: Character index out of range")
                         : WebKit::Fail(cx, "Character index out of range");
    const size_t end = std::min<size_t>(measurement.characters.size(), size_t(index) + count);
    double total = 0;
    for (size_t i = index; i < end; ++i)
      // A range that starts inside a cluster (a low surrogate) skips that cluster.
      if (i == 0 || measurement.characters[i] != measurement.characters[i - 1]) total += measurement.characters[i][2];
    args.rval().setNumber(double(float(total)));
    return true;
  }
  if (method == Extent && (!rendered || index >= measurement.characters.size())) {
    const char* name = rendered ? "IndexSizeError" : "InvalidStateError";
    const char* message = rendered ? "SVGTextContentElement.getExtentOfChar: Character index out of range"
                                  : "SVGTextContentElement.getExtentOfChar: Element has no text frame";
    return s.exception ? s.exception(s.exceptionContext, name, message) : WebKit::Fail(cx, message);
  }
  auto* factory = static_cast<Realm*>(node.factory);
  if (!factory) return WebKit::Fail(cx, "SVG geometry original factory is not installed");
  return factory->NewSvgRect((method == Extent ? measurement.characters[index] : measurement.box).data(),
                             element, args.rval());
}
}
bool Realm::InstallSvgProjection(JS::HandleObject graphics, JS::HandleObject text, const SvgPorts& ports) {
  State& s = *state_; s.svgPorts = ports;
  JSAutoRealm realm(s.cx, s.kit.page);
  WebFace face(s.cx); face.prototype = graphics;
  if (!s.kit.Method(face, "SVGGraphicsElement", "getBBox", Call, 0, BBox)) return false;
  face.prototype = text;
  return s.kit.Method(face, "SVGTextContentElement", "getNumberOfChars", Call, 0, Count) &&
      s.kit.Method(face, "SVGTextContentElement", "getComputedTextLength", Call, 0, Length) &&
      s.kit.Method(face, "SVGTextContentElement", "getSubStringLength", Call, 2, SubString) &&
      s.kit.Method(face, "SVGTextContentElement", "getExtentOfChar", Call, 1, Extent);
}
}
