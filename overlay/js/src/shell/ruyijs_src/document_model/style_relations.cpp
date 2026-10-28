// Read relation metadata from the receiver's original DOM owner on demand.
#include "document_model/realm.h"
#include "document_model/state.h"
#include "document_model/style_source_values.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
namespace ruyijs::document_model {
namespace {
StyleIdentity Identity(NodeKey key) {return {key.document,key.node};}
bool ElementAlong(State& state,NodeKey key,bool forward,StyleIdentity* out) {
  while(key.node) {
    NodeSnapshot node;if(!state.Check(state.model.Inspect(key,&node))) return false;
    if(node.kind==NodeKind::Element) {*out=Identity(key);return true;}
    key=forward?node.next_sibling:node.previous_sibling;
  }
  return true;
}
}
bool Realm::ReadStyleRelation(JS::HandleObject element,uint64_t document,uint64_t key,StyleRelationFrame* out) {
  JSContext* cx=state_->cx;JS::RootedObject receiver(cx,js::CheckedUnwrapStatic(element));
  JS::RootedValue value(cx,receiver?JS::ObjectValue(*receiver):JS::NullValue());
  auto* entry=NodeReceiver(cx,value);if(!entry) return false;
  State& state=*entry->owner;JSAutoRealm realm(cx,state.page);
  NodeKey current=key?NodeKey{document,key}:entry->key;
  NodeSnapshot node;if(!state.Check(state.model.Inspect(current,&node))) return false;
  if(node.kind!=NodeKind::Element) return TypeError(cx,"CSS relation input requires an Element");
  *out=StyleRelationFrame{};out->identity=Identity(current);out->parent=Identity(node.parent_element);
  out->html=node.is_html_element&&node.document_mode==DocumentMode::Html;
  out->value.tag=StyleSourceText(*node.local_name);
  if(!ElementAlong(state,node.previous_sibling,false,&out->previous)||
      !ElementAlong(state,node.next_sibling,true,&out->next)||
      !ElementAlong(state,node.first_child,true,&out->first)) return false;
  std::vector<AttributeSnapshot> attributes;
  if(!state.Check(state.model.Attributes(current,&attributes))) return false;
  for(const auto& attribute:attributes) {
    if(!attribute.namespace_uri.empty()) continue;
    auto name=StyleSourceText(attribute.name),text=StyleSourceText(attribute.value);
    if(name=="id") out->value.id=text;
    if(name=="style") out->value.inline_style=text;
    if(name=="class") {
      for(size_t p=0;p<text.size();) {
        while(p<text.size()&&StyleSourceSpace(text[p])) ++p;
        size_t end=p;while(end<text.size()&&!StyleSourceSpace(text[end])) ++end;
        if(end>p) out->value.classes.push_back(text.substr(p,end-p));p=end;
      }
    }
    out->value.attributes.emplace_back(std::move(name),std::move(text));
  }
  out->empty=!out->first.key;
  for(auto child=node.first_child;out->empty&&child.node;) {
    NodeSnapshot item;if(!state.Check(state.model.Inspect(child,&item))) return false;
    if(item.kind==NodeKind::Text&&item.data_length) out->empty=false;
    child=item.next_sibling;
  }
  if(node.parent.node&&!node.parent_element.node) {
    NodeSnapshot parent;if(!state.Check(state.model.Inspect(node.parent,&parent))) return false;
    out->root=parent.kind==NodeKind::Document;
  }
  if(node.is_connected) {
    DocumentSnapshot doc;if(!state.Check(state.model.InspectDocument(node.owner_document,&doc))) return false;
    out->focused=doc.active_element.node==current.node&&doc.active_element.document==current.document;
  }
  return true;
}
}
