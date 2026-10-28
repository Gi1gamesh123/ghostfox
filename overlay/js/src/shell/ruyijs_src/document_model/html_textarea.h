#pragma once
#include "document_model/html_control.h"

namespace ruyijs::document_model {
bool InstallTextareaMethods(State&);
bool TextareaLabels(State&, JS::MutableHandleValue);
}
