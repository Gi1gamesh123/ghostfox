#pragma once
#include "web_styling/cssom.h"
#include "document_model/style_port.h"
#include <string_view>
namespace ruyijs::web_styling {
extern const JSClass kPageDescriptorsClass;
const js::BaseProxyHandler* PageDescriptorsHandler();
inline JSObject* DeclarationStorage(JSObject* object) {
  return JS::GetClass(object)==&kPageDescriptorsClass?&js::GetProxyPrivate(object).toObject():object;
}
inline std::vector<ruyijs::domain::styling::CssDeclarationValue>& PageDeclarationRows(
    JSContext* cx,JS::HandleObject view) {
  JS::RootedObject rule(cx,&JS::GetReservedSlot(DeclarationStorage(view),2).toObject());
  return RuleDataOf(rule).PageDeclarations();
}
bool OwnsDeclaration(JSObject*);
bool CallPageDeclaration(JSContext*,document_model::StyleMember,JS::HandleObject,
                          const JS::CallArgs&,const char*,const char*);
bool InstallPageRules(State&);
bool InstallPageDescriptors(State&);
bool KnownDeclarationName(std::string_view);
}
