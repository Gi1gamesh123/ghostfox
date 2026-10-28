#pragma once
#include <string>
#include <string_view>
namespace ruyijs::domain::styling {
enum CssNumericKind { CssInvalid=0, CssNumber=1, CssLength=2, CssPercent=4,
                      CssAngle=8, CssTime=16, CssResolution=32 };
struct CssNumericValue { int kind=CssInvalid;double number=0;bool expression=false; };
bool CssNumberToken(std::string_view,size_t*,double*);
CssNumericValue CssNumericParse(std::string_view);
CssNumericValue CssNumericResolve(std::string_view, double em, double rem, double percent);
bool CssNumericAllowed(std::string_view,int kinds,bool negative=true,bool integer=false);
}
