// The legacy SVG DOM value types SVGAngle/SVGNumber/SVGMatrix/SVGTransform/
// SVGPreserveAspectRatio, their SVGAnimated* wrappers and the four SVG*List
// interfaces. Window-only; the list and animated instances are not yet wired
// to element attributes (see docs/DEVIATIONS.md).
#pragma once
#include "web_geometry/internal.h"

namespace ruyijs::web_geometry {
// Concrete value classes.
extern const JSClass kSvgNumberClass, kSvgAngleClass, kSvgMatrixClass, kSvgTransformClass,
    kSvgPreserveAspectRatioClass;
// SVGAnimated*: primitive baseVal/animVal and object baseVal/animVal classes.
extern const JSClass kSvgAnimatedBooleanClass, kSvgAnimatedEnumerationClass, kSvgAnimatedIntegerClass,
    kSvgAnimatedNumberClass, kSvgAnimatedAngleClass, kSvgAnimatedLengthListClass,
    kSvgAnimatedNumberListClass, kSvgAnimatedPreserveAspectRatioClass, kSvgAnimatedTransformListClass;
// The four list interfaces (brand-only classes; no public instances yet).
extern const JSClass kSvgLengthListClass, kSvgNumberListClass, kSvgStringListClass, kSvgTransformListClass;

bool InstallSvgDomTypes(State&);
bool InstallSvgNumber(State&);
bool InstallSvgAngle(State&);
bool InstallSvgMatrix(State&);
bool InstallSvgTransform(State&);
bool InstallSvgPreserveAspectRatio(State&);
bool InstallSvgAnimated(State&);
bool InstallSvgLists(State&);

bool SvgBrandReceiver(JSContext*, JS::HandleValue, const JSClass*, const char* member,
                      JS::MutableHandleObject);
bool NewSvgNumberObject(State&, double, JS::MutableHandleObject);
bool NewSvgAngleObject(State&, double degrees, unsigned unit, JS::MutableHandleObject);
bool NewSvgMatrixObject(State&, const double components[6], JS::MutableHandleObject);
bool NewSvgTransformObject(State&, unsigned type, const double components[6], double angle,
                           JS::MutableHandleObject);
}  // namespace ruyijs::web_geometry
