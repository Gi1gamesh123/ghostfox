#pragma once
#include "document_model/html_control.h"

namespace ruyijs::document_model {
bool FormResolve(JSContext*, JS::HandleObject, JS::HandleId, bool*);
bool FormEnumerate(JSContext*, JS::HandleObject, JS::MutableHandleIdVector, bool);
Collection* FormCollection(State&, NodeKey, CollectionFilter filter = CollectionFilter::FormControls,
                           DomStringView name = u"");
bool FormNamed(State&, NodeKey, DomStringView name, JS::MutableHandleValue);
bool MaterializeForm(State&, JS::HandleObject, CollectionRecord&);
bool InstallFormCollections(State&);
bool InstallFormMembers(State&);
}
