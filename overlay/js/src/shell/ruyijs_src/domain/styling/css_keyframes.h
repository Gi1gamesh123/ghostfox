// Keyframe grammar and serialization over the existing canonical rule value.
#pragma once
#include "domain/styling/css_rule_value.h"
namespace ruyijs::domain::styling {
bool CssKeyText(const std::string&, std::string*);
bool CssKeyPositions(const std::string&, std::vector<float>*);
std::string CssKeyText(const std::vector<float>&);
bool CssKeyframesName(const std::string&, std::string*);
std::string CssKeyframesNameText(const std::string&);
std::string CssKeyframeDeclarations(const std::string&);
CssRuleParseResult ParseCssKeyframes(const std::string&, bool strict = false);
}
