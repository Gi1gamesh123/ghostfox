#pragma once
#include "web_geometry/internal.h"
#include "domain/geometry/matrix.h"
namespace ruyijs::web_geometry {
extern const JSClass kMatrixClass, kMatrixReadOnlyClass;
inline constexpr const char* kMatrixFields[] = {"a", "b", "c", "d", "e", "f",
  "m11", "m12", "m13", "m14", "m21", "m22", "m23", "m24",
  "m31", "m32", "m33", "m34", "m41", "m42", "m43", "m44", "is2D", "isIdentity"};
inline unsigned MatrixIndex(unsigned field) {
  return field < 6 ? domain::geometry::kAliases[field] : field - 6;
}
enum MatrixOperation { NewIdentity, SameMatrix, TranslateMatrix, SetMatrixString,
                        MatrixFloat32, MatrixFloat64, MatrixPoint, MatrixJSON, MatrixString };
bool MatrixReceiver(JSContext*, JS::HandleValue, bool readonly, JS::MutableHandleObject);
void ReadMatrix(JS::HandleObject, double*);
bool NewMatrix(State&, bool readonly, const double*, bool is2D,
                const JS::CallArgs*, JS::MutableHandleValue);
bool MatrixConstruct(JSContext*, unsigned, JS::Value*, bool readonly);
bool MatrixGet(JSContext*, unsigned, JS::Value*);
bool MatrixSet(JSContext*, unsigned, JS::Value*);
bool MatrixFrom(JSContext*, unsigned, JS::Value*);
bool MatrixMethod(JSContext*, unsigned, JS::Value*);
bool MatrixExport(State&, JS::HandleObject, MatrixOperation, JS::MutableHandleValue);
bool InstallMatrix(State&);
}
