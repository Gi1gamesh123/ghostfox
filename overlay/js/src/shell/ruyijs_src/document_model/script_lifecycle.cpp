// Adapt committed DOM changes to the one application script coordinator.
#include "document_model/state.h"
#include "js/Exception.h"
#include "js/Realm.h"
namespace ruyijs::document_model {
namespace {
void Changed(void* context,NodeKey key,bool subtree) {
  auto& s=*static_cast<State*>(context);
  if(!s.scriptPort || key.document!=s.main.document || JS_IsExceptionPending(s.cx))return;
  if(s.model.IdentityTransferPending()) {
    for(auto pending:s.deferredScriptScopes)if(Same(pending,key))return;
    s.deferredScriptScopes.push_back(key);return;
  }
  std::vector<NodeKey> nodes;
  if(subtree && !s.Check(s.model.Subtree(key,&nodes)))return;
  nodes.insert(nodes.begin(),key);
  JSAutoRealm realm(s.cx,s.page);
  for(auto node:nodes) {
    NodeSnapshot snapshot;
    if(!s.Check(s.model.Inspect(node,&snapshot)))return;
    if(snapshot.kind==NodeKind::Text)node=snapshot.parent;
    if(!node.node)continue;
    bool execute=false,external=false;
    if(!s.Check(s.model.PrepareScript(node,false,&execute,&external)))return;
    if(!execute)continue;
    if(s.scriptDepth>=32){TypeError(s.cx,"Script nesting limit exceeded");return;}
    JS::RootedValue value(s.cx);
    if(!s.Wrap(node,&value))return;
    JS::RootedObject script(s.cx,&value.toObject());
    ++s.scriptDepth;
    const bool ok=s.scriptPort(s.scriptContext,*s.sourceRealm,s.interaction,s.environment,s.page,script,external);
    --s.scriptDepth;
    if(!ok)return;
  }
}
}
void State::FlushScriptChanges() {
  auto scopes=std::move(deferredScriptScopes);deferredScriptScopes.clear();
  for(auto key:scopes)Changed(this,key,true);
}
void Realm::SetScriptPort(ScriptPort port,void* context) {
  auto& s=*state_;++s.scriptGeneration;s.scriptPort=port;s.scriptContext=context;
  s.model.SetScriptSink(port?Changed:nullptr,port?&s:nullptr);
}
void Realm::CopyScriptPortFrom(const Realm& source) {
  SetScriptPort(source.state_->scriptPort,source.state_->scriptContext);
}
uint64_t Realm::ScriptGeneration() const {return state_->scriptGeneration;}
bool Realm::CurrentScript(JS::MutableHandleObject result) {
  auto& s=*state_;DocumentSnapshot snapshot;JS::RootedValue value(s.cx);
  if(!s.Check(s.model.InspectDocument(s.main,&snapshot))||!s.Wrap(snapshot.current_script,&value))return false;
  result.set(value.isObject()?&value.toObject():nullptr);return true;
}
}
