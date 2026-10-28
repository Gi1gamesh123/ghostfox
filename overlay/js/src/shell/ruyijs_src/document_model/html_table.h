#pragma once
#include "document_model/state.h"

namespace ruyijs::document_model {
enum TableBrand { Table, Section, Row, Cell, Col, Caption };
const char* TableInterface(TableBrand);
Wrapper* TableReceiver(JSContext*, const JS::CallArgs&, TableBrand);
bool TableCreate(State&, NodeKey scope, DomStringView tag, JS::MutableHandleValue, NodeKey*);
bool TableMove(State&, NodeKey parent, NodeKey child, NodeKey before);
bool InstallTableRows(State&);
bool InstallTableParts(State&);
bool InstallTableAttributes(State&);
}
