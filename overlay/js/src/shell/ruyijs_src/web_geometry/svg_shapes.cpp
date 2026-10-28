#include "web_geometry/svg.h"
#include "web_geometry/svg_path.h"
#include "domain/styling/css_numeric_value.h"
#include <cstdlib>

namespace ruyijs::web_geometry {
bool SvgShape(const SvgInput& input, const SvgOptions& options, svg::SvgBox* box, bool* present) {
  *box = {}; *present = false;
  auto strokeText = input.style.Get("stroke");
  if (strokeText.empty()) strokeText = SvgAttribute(input,"stroke");
  const bool stroke = options.stroke && !strokeText.empty() && strokeText != "none";
  if (!options.fill && !stroke) return true;
  auto widthText = input.style.Get("stroke-width");
  if (widthText.empty()) widthText=SvgAttribute(input,"stroke-width");
  const auto parsed = ruyijs::domain::styling::CssNumericResolve(widthText,input.style.fontSize,16,
      std::hypot(input.viewportWidth,input.viewportHeight)/std::sqrt(2.0));
  const float width = stroke ? float(parsed.kind ? std::max(0.0,parsed.number) : 1) : 0;
  const auto& name = input.node.localName;
  const auto horizontal = [&](const char* key) { return float(SvgLength(input,key,input.viewportWidth)); };
  const auto vertical = [&](const char* key) { return float(SvgLength(input,key,input.viewportHeight)); };
  if (name == u"rect" || name == u"image" || name == u"foreignObject") {
    const float w=horizontal("width"), h=vertical("height");
    if (w <= 0 || h <= 0) return true;
    *box={horizontal("x")-width/2,vertical("y")-width/2,w+width,h+width};
  } else if (name == u"circle" || name == u"ellipse") {
    const float rx=name==u"circle"?float(SvgLength(input,"r",std::hypot(input.viewportWidth,input.viewportHeight)/std::sqrt(2.0))):horizontal("rx");
    const float ry=name==u"circle"?rx:vertical("ry");
    if (rx <= 0 || ry <= 0) return true;
    *box={horizontal("cx")-rx-width/2,vertical("cy")-ry-width/2,2*rx+width,2*ry+width};
  } else {
    std::vector<svg::SvgPathCommand> path;
    if (name == u"line") {
      path={{'M',{horizontal("x1"),vertical("y1")}},{'L',{horizontal("x2"),vertical("y2")}}};
    } else if (name == u"polyline" || name == u"polygon") {
      const auto points=SvgAttribute(input,"points"); const char* p=points.c_str();
      while (*p && path.size()<65536) {
        float pair[2];
        for (unsigned i=0;i<2;++i) {
          while (*p==',' || *p==' ' || *p=='\t' || *p=='\n') ++p;
          char* end=nullptr; pair[i]=float(std::strtod(p,&end));
          if (end==p || !std::isfinite(pair[i])) return false;
          p=end;
        }
        path.push_back({path.empty()?'M':'L',{pair[0],pair[1]}});
      }
      if (!path.empty() && name==u"polygon") path.push_back({'Z'});
    } else if (name == u"path") {
      if (!svg::ParseSvgPath(SvgAttribute(input,"d"),&path)) return false;
    } else return false;
    if (path.empty()) return true;
    if (!SvgPathBounds(path,width,box)) return false;
  }
  *present=true; return true;
}
}
