#pragma once
#include "web_geometry/svg.h"
#include "domain/geometry/svg_length_value.h"
#include "domain/document_model/svg_length_fields.h"
namespace ruyijs::web_geometry {
using SvgLengthValue = domain::geometry::SvgLengthValue;
enum SvgLengthSlot { LengthNumber, LengthUnit, LengthNode, LengthField, LengthMode };
extern const JSClass kSvgLengthClass;
bool InstallSvgLength(State&);
bool InstallSvgAnimatedLength(State&);
bool SvgLengthGet(JSContext*, unsigned, JS::Value*);
bool SvgLengthSet(JSContext*, unsigned, JS::Value*);
bool SvgLengthCall(JSContext*, unsigned, JS::Value*);
bool SvgLengthReceiver(JSContext*, JS::HandleValue, const std::string&, JS::MutableHandleObject);
bool SvgLengthRead(State&, JS::HandleObject, SvgLengthValue*);
bool SvgLengthWrite(State&, JS::HandleObject, const SvgLengthValue&);
bool SvgLengthFactor(State&, JS::HandleObject, unsigned, float*);
bool SvgLengthException(State&, const char*, const std::string&);
bool SvgLengthWritable(State&, JS::HandleObject, const std::string&);
bool NewSvgLengthObject(State&, JS::HandleObject, unsigned, unsigned, JS::MutableHandleObject);
bool SvgLengthFloat(JSContext*, JS::HandleValue, const std::string&, float*);
}
