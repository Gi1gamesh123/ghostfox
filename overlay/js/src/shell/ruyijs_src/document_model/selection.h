#pragma once
#include "js/TypeDecls.h"

namespace ruyijs::document_model {
struct State;
extern const JSClass kSelectionClass;
bool InstallSelection(State&);
bool InstallSelectionMembers(State&);
}
