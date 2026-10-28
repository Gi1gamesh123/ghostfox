#include "web_canvas/path_geometry.h"
#include <algorithm>
#include <cmath>

namespace ruyijs::web_canvas {
namespace {
struct Point { double x, y; };
struct Projection {
  std::vector<std::vector<Point>> paths;
  double matrix[6] = {1, 0, 0, 1, 0, 0};
  Point Map(double x, double y) const {
    return {matrix[0]*x + matrix[2]*y + matrix[4],
            matrix[1]*x + matrix[3]*y + matrix[5]};
  }
  void Move(Point p) { paths.push_back({p}); }
  void Line(Point p) {
    if (paths.empty()) Move(p); else paths.back().push_back(p);
  }
  void Close() {
    if (paths.empty() || paths.back().empty()) return;
    Point first = paths.back().front();
    paths.back().push_back(first);
    Move(first);
  }
  void Curve(Point a, Point b, Point end, bool cubic) {
    if (paths.empty()) Move(a);
    Point start = paths.back().back();
    for (int i = 1; i <= 128; ++i) {
      double t = i/128.0, u = 1-t;
      Line(cubic ? Point{u*u*u*start.x+3*u*u*t*a.x+3*u*t*t*b.x+t*t*t*end.x,
                         u*u*u*start.y+3*u*u*t*a.y+3*u*t*t*b.y+t*t*t*end.y}
                 : Point{u*u*start.x+2*u*t*a.x+t*t*end.x,
                         u*u*start.y+2*u*t*a.y+t*t*end.y});
    }
  }
  void Arc(const double* p, bool ellipse) {
    constexpr double tau = 6.28318530717958647692;
    double rx=p[2], ry=ellipse?p[3]:p[2], rotation=ellipse?p[4]:0;
    double start=p[ellipse?5:3], end=p[ellipse?6:4];
    bool ccw=p[ellipse?7:5]!=0;
    double delta=end-start;
    if ((!ccw && delta>=tau) || (ccw && -delta>=tau)) delta=ccw?-tau:tau;
    else {
      delta=std::fmod(delta,tau);
      if (!ccw && delta<0) delta+=tau;
      if (ccw && delta>0) delta-=tau;
    }
    for (int i=0;i<=128;++i) {
      double angle=start+delta*i/128.0, x=rx*std::cos(angle), y=ry*std::sin(angle);
      Line(Map(p[0]+x*std::cos(rotation)-y*std::sin(rotation),
               p[1]+x*std::sin(rotation)+y*std::cos(rotation)));
    }
  }
};
}
bool PathContains(const std::vector<double>& commands, double x, double y, bool evenodd) {
  Projection shape;
  const unsigned widths[] = {2,2,0,4,6,8,4,6,6};
  for (size_t at=0;at<commands.size();) {
    unsigned op=unsigned(commands[at++]);
    if (op>8 || at+widths[op]>commands.size()) return false;
    const double* p=commands.data()+at; at+=widths[op];
    switch(op) {
      case 0: shape.Move(shape.Map(p[0],p[1])); break;
      case 1: shape.Line(shape.Map(p[0],p[1])); break;
      case 2: shape.Close(); break;
      case 3:
        shape.Move(shape.Map(p[0],p[1]));
        shape.Line(shape.Map(p[0]+p[2],p[1]));
        shape.Line(shape.Map(p[0]+p[2],p[1]+p[3]));
        shape.Line(shape.Map(p[0],p[1]+p[3])); shape.Close(); break;
      case 4: shape.Arc(p,false); break;
      case 5: shape.Arc(p,true); break;
      case 6: shape.Curve(shape.Map(p[0],p[1]),{},shape.Map(p[2],p[3]),false); break;
      case 7: shape.Curve(shape.Map(p[0],p[1]),shape.Map(p[2],p[3]),
                          shape.Map(p[4],p[5]),true); break;
      case 8: std::copy(p,p+6,shape.matrix); break;
    }
  }
  int winding=0;
  for (const auto& path:shape.paths) {
    if (path.size()<2) continue;
    for (size_t i=0;i<path.size();++i) {
      Point a=path[i],b=path[(i+1)%path.size()];
      if (a.x==b.x && a.y==b.y) continue;
      double cross=(b.x-a.x)*(y-a.y)-(x-a.x)*(b.y-a.y);
      if (std::abs(cross)<=1e-9 && x>=std::min(a.x,b.x) && x<=std::max(a.x,b.x) &&
          y>=std::min(a.y,b.y) && y<=std::max(a.y,b.y)) return true;
      if (a.y<=y && b.y>y && cross>0) ++winding;
      if (a.y>y && b.y<=y && cross<0) --winding;
    }
  }
  return evenodd ? (winding%2)!=0 : winding!=0;
}
}
