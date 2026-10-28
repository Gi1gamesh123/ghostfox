// Read the existing realm's configured display source for all Styling conditions.
#pragma once
#include "domain/styling/css_condition_value.h"
namespace ruyijs::web_styling {
struct State;
bool ReadConditionEnvironment(State&,ruyijs::domain::styling::CssMediaEnvironment*);
}
