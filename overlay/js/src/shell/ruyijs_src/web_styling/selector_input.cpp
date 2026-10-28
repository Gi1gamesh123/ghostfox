#include "web_styling/selector_input.h"
#include "web_styling/state.h"
namespace ruyijs::web_styling {
SelectorInput::SelectorInput(State& state,JS::HandleObject element):state_(state),element_(state.cx,element) {}
const ruyijs::domain::styling::ElementNode* SelectorInput::Load(uint64_t document,uint64_t key) const {
  if(!ok_) return nullptr;
  auto it=entries_.find({document,key});if(it!=entries_.end()) return &it->second.node;
  if(entries_.size()>=100000) {ok_=false;WebKit::Fail(state_.cx,"CSS relation input limit exceeded");return nullptr;}
  document_model::StyleRelationFrame frame;
  if(!state_.relationPort||!state_.relationPort(state_.chainContext,element_,document,key,&frame)) {ok_=false;return nullptr;}
  Entry entry;auto& node=entry.node;
  node.document=frame.identity.document;node.key=frame.identity.key;node.relations=this;
  node.tag=std::move(frame.value.tag);node.id=std::move(frame.value.id);
  node.inline_style=std::move(frame.value.inline_style);node.classes=std::move(frame.value.classes);
  for(auto& attr:frame.value.attributes) node.attributes.emplace(std::move(attr.first),std::move(attr.second));
  frame.value={};node.root=frame.root;node.empty=frame.empty;node.html=frame.html;node.focused=frame.focused;
  auto identity=std::make_pair(node.document,node.key);entry.frame=std::move(frame);
  return &entries_.emplace(identity,std::move(entry)).first->second.node;
}
const ruyijs::domain::styling::ElementNode* SelectorInput::Related(
    const ruyijs::domain::styling::ElementNode& node,char relation) const {
  const auto& frame=entries_.at({node.document,node.key}).frame;
  auto key=relation=='>'?frame.parent:relation=='-'?frame.previous:relation=='+'?frame.next:frame.first;
  return key.key?Load(key.document,key.key):nullptr;
}
}
