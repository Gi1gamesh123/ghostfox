#include "web_geometry/internal.h"
#include "web_geometry/matrix.h"
#include "web_geometry/rect_list.h"
#include "web_geometry/svg.h"
#include "web_geometry/svg_dom_types.h"
#include "web_geometry/svg_length.h"
#include "web_geometry/svg_points.h"
#include "runtime/page_helpers.h"
#include "js/Realm.h"

namespace ruyijs::web_geometry {
namespace {
bool RectROCtor(JSContext* c, unsigned n, JS::Value* p) { return Construct(c, n, p, RectReadOnly); }
bool RectCtor(JSContext* c, unsigned n, JS::Value* p) { return Construct(c, n, p, Rect); }
bool PointROCtor(JSContext* c, unsigned n, JS::Value* p) { return Construct(c, n, p, PointReadOnly); }
bool PointCtor(JSContext* c, unsigned n, JS::Value* p) { return Construct(c, n, p, Point); }
bool QuadCtor(JSContext* c, unsigned n, JS::Value* p) { return Construct(c, n, p, Quad); }
bool Static(State& s, Kind kind, const char* name, JSNative native, int slot) {
  JS::RootedObject fn(s.cx, s.kit.Function(native, name, 0, slot));
  return fn && JS_DefineProperty(s.cx, s.faces[kind].constructor, name, fn, JSPROP_ENUMERATE) &&
         s.kit.helpers->Member(fn, kNames[kind], name, "call");
}
}
Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers)
    : state_(std::make_unique<State>(cx, page, helpers)) {}
Realm::~Realm() = default;
void Realm::SetExceptionPort(bool (*port)(void*, const char*, const char*), void* context) {
  state_->exception = port; state_->exceptionContext = context;
}
JSObject* Realm::ReadOnlyRectPrototype() const { return state_->faces[RectReadOnly].prototype; }
bool Realm::InstallWorkerValues() {
  state_->worker = true;
  return InstallValues();
}
bool Realm::InstallValues() {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.kit.page);
  JS::RootedObject plain(s.cx, JS::GetRealmObjectPrototype(s.cx));
  const JSClass* classes[] = {&kRectReadOnlyClass, &kRectClass, &kPointReadOnlyClass, &kPointClass, &kQuadClass};
  JSNative ctors[] = {RectROCtor, RectCtor, PointROCtor, PointCtor, QuadCtor};
  for (unsigned i = 0; i < 5; ++i) {
    bool derived = i == Rect || i == Point;
    JS::RootedObject parent(s.cx, derived ? s.faces[i - 1].prototype.get() : plain.get());
    if (!s.kit.Define(kNames[i], classes[i], parent, ctors[i], &s.faces[i])) return false;
    if (derived && !JS_SetPrototype(s.cx, s.faces[i].constructor, s.faces[i - 1].constructor)) return false;
  }
  for (unsigned i = 0; i < 4; ++i) {
    Kind kind = Kind(i);
    bool point = i >= PointReadOnly, writable = i == Rect || i == Point;
    unsigned count = i == RectReadOnly ? 8 : 4;
    const char* const* names = point ? kPointFields : kRectFields;
    for (unsigned j = 0; j < count; ++j)
      if (!s.kit.Accessor(s.faces[i], kNames[i], names[j], ScalarGet,
                          writable ? ScalarSet : nullptr, i * 16 + j)) return false;
    if (!writable && !s.kit.Method(s.faces[i], kNames[i], "toJSON", ScalarJSON, 0, i)) return false;
    if (!Static(s, kind, point ? "fromPoint" : "fromRect", ScalarFrom, i)) return false;
  }
  if (!s.kit.Method(s.faces[PointReadOnly], kNames[PointReadOnly], "matrixTransform", TransformPoint, 0, 0)) return false;
  const char* points[] = {"p1", "p2", "p3", "p4"};
  for (unsigned i = 0; i < 4; ++i)
    if (!s.kit.Accessor(s.faces[Quad], kNames[Quad], points[i], QuadGet, nullptr, i)) return false;
  return s.kit.Method(s.faces[Quad], kNames[Quad], "toJSON", QuadJSON, 0, 0) &&
         s.kit.Method(s.faces[Quad], kNames[Quad], "getBounds", QuadBounds, 0, 0) &&
         Static(s, Quad, "fromQuad", QuadFrom, 0) && Static(s, Quad, "fromRect", QuadFrom, 1) &&
         InstallMatrix(s);
}
bool Realm::Install() {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.kit.page);
  return InstallValues() && InstallRectList(s) && InstallSvgRect(s) &&
      InstallSvgAnimatedRect(s) && InstallSvgUnitTypes(s) && InstallSvgPoints(s) && InstallSvgLength(s) &&
      InstallSvgDomTypes(s);
}
}
