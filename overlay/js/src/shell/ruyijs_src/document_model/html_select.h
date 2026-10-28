#pragma once
#include "document_model/html_control.h"

namespace ruyijs::document_model {
bool SelectOptions(Wrapper&, JS::MutableHandleValue);
bool SelectWillValidate(Wrapper&, bool*);
bool InstallSelectAttributes(State&);
bool InstallSelectMethods(State&);
bool InstallOptionsCollection(State&);
}
