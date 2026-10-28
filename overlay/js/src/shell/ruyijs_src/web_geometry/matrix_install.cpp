#include "web_geometry/matrix.h"
#include "runtime/page_helpers.h"
#include "js/Realm.h"
#include <cstring>

namespace ruyijs::web_geometry {
namespace {
bool ReadOnlyCtor(JSContext* c, unsigned n, JS::Value* p) { return MatrixConstruct(c, n, p, true); }
bool MutableCtor(JSContext* c, unsigned n, JS::Value* p) { return MatrixConstruct(c, n, p, false); }
}
bool InstallMatrix(State& s) {
  JS::RootedObject plain(s.cx, JS::GetRealmObjectPrototype(s.cx));
  if (!s.kit.Define("DOMMatrixReadOnly", &kMatrixReadOnlyClass, plain, ReadOnlyCtor, &s.matrixReadOnly) ||
      !s.kit.Define("DOMMatrix", &kMatrixClass, s.matrixReadOnly.prototype, MutableCtor, &s.matrix) ||
      !JS_SetPrototype(s.cx, s.matrix.constructor, s.matrixReadOnly.constructor) ||
      (!s.worker && !JS_DefineProperty(s.cx, s.kit.page, "WebKitCSSMatrix", s.matrix.constructor, 0))) return false;
  for (unsigned kind = 0; kind < 2; ++kind) {
    WebFace& face = kind ? s.matrix : s.matrixReadOnly;
    const char* name = kind ? "DOMMatrix" : "DOMMatrixReadOnly";
    for (unsigned i = 0; i < (kind ? 22u : 24u); ++i)
      if (!s.kit.Accessor(face, name, kMatrixFields[i], MatrixGet, kind ? MatrixSet : nullptr, kind * 32 + i)) return false;
    const char* factories[] = {"fromMatrix", "fromFloat32Array", "fromFloat64Array"};
    for (unsigned i = 0; i < 3; ++i) {
      JS::RootedObject fn(s.cx, s.kit.Function(MatrixFrom, factories[i], i ? 1 : 0, kind));
      if (!fn || !JS_DefineProperty(s.cx, face.constructor, factories[i], fn, JSPROP_ENUMERATE) ||
          !s.kit.helpers->Member(fn, name, factories[i], "call")) return false;
    }
  }
  struct Method { const char* name; MatrixOperation op; bool mutableOnly; unsigned length; };
  const Method methods[] = {
    {"flipX", NewIdentity, false, 0}, {"flipY", NewIdentity, false, 0},
    {"inverse", NewIdentity, false, 0}, {"multiply", NewIdentity, false, 0},
    {"rotate", NewIdentity, false, 0}, {"rotateAxisAngle", NewIdentity, false, 0},
    {"rotateFromVector", NewIdentity, false, 0}, {"scale", NewIdentity, false, 0},
    {"scale3d", NewIdentity, false, 0}, {"scaleNonUniform", NewIdentity, false, 0},
    {"skewX", NewIdentity, false, 0}, {"skewY", NewIdentity, false, 0},
    {"translate", NewIdentity, false, 0}, {"toFloat32Array", MatrixFloat32, false, 0},
    {"toFloat64Array", MatrixFloat64, false, 0}, {"toJSON", MatrixJSON, false, 0},
    {"toString", MatrixString, false, 0}, {"transformPoint", MatrixPoint, false, 0},
    {"multiplySelf", SameMatrix, true, 0}, {"preMultiplySelf", SameMatrix, true, 0},
    {"rotateSelf", SameMatrix, true, 0}, {"rotateAxisAngleSelf", SameMatrix, true, 0},
    {"rotateFromVectorSelf", SameMatrix, true, 0}, {"scaleSelf", SameMatrix, true, 0},
    {"scale3dSelf", SameMatrix, true, 0}, {"skewXSelf", SameMatrix, true, 0},
    {"skewYSelf", SameMatrix, true, 0}, {"invertSelf", SameMatrix, true, 0},
    {"translateSelf", TranslateMatrix, true, 0}, {"setMatrixValue", SetMatrixString, true, 1}
  };
  for (const auto& method : methods)
    if ((!s.worker || (std::strcmp(method.name, "toString") && std::strcmp(method.name, "setMatrixValue"))) && !s.kit.Method(method.mutableOnly ? s.matrix : s.matrixReadOnly,
                      method.mutableOnly ? "DOMMatrix" : "DOMMatrixReadOnly",
                      method.name, MatrixMethod, method.length, method.op)) return false;
  return true;
}
}
