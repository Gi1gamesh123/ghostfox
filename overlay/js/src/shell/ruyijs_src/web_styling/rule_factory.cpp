// One rule factory over canonical domain values, cached by its original parent.
#include "web_styling/cssom.h"
#include "js/Array.h"
#include "js/Wrapper.h"
#include <new>
namespace ruyijs::web_styling {
namespace {
void FinalizeRule(JS::GCContext*,JSObject* object) {
  const auto& value=RuleSlotValue(object,RuleData);
  if(!value.isUndefined()) delete static_cast<std::shared_ptr<SheetRule>*>(value.toPrivate());
}
const JSClassOps ruleOps={nullptr,nullptr,nullptr,nullptr,nullptr,
                          nullptr,FinalizeRule,nullptr,nullptr,nullptr};
}
const JSClass kRuleClass={"CSSRule",JSCLASS_HAS_RESERVED_SLOTS(8),&ruleOps};
const JSClass kStyleRuleClass={"CSSStyleRule",JSCLASS_HAS_RESERVED_SLOTS(8),&ruleOps};
const JSClass kPageRuleClass={"CSSPageRule",JSCLASS_HAS_RESERVED_SLOTS(8),&ruleOps};
const JSClass kGroupingRuleClass={"CSSGroupingRule",JSCLASS_HAS_RESERVED_SLOTS(8),&ruleOps};
const JSClass kConditionRuleClass={"CSSConditionRule",JSCLASS_HAS_RESERVED_SLOTS(8),&ruleOps};
const JSClass kMediaRuleClass={"CSSMediaRule",JSCLASS_HAS_RESERVED_SLOTS(8),&ruleOps};
const JSClass kSupportsRuleClass={"CSSSupportsRule",JSCLASS_HAS_RESERVED_SLOTS(8),&ruleOps};
const JSClass kNestedDeclarationsClass={"CSSNestedDeclarations",JSCLASS_HAS_RESERVED_SLOTS(8),&ruleOps};
const JSClass kKeyframeRuleClass={"CSSKeyframeRule",JSCLASS_HAS_RESERVED_SLOTS(8),&ruleOps};
bool IsRuleObject(JS::HandleObject object) {
  if(!object) return false;
  const auto* c=JS::GetClass(object);
  return c==&kStyleRuleClass||c==&kMediaRuleClass||c==&kSupportsRuleClass||c==&kNestedDeclarationsClass||
    c==&kKeyframeRuleClass||c==&kKeyframesRuleClass||c==&kPageRuleClass;
}
bool RuleReceiver(JSContext* cx,JS::HandleValue value,JS::MutableHandleObject out,
                    bool styleOnly,bool conditionOnly) {
  out.set(value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr);
  if(!IsRuleObject(out)||(styleOnly&&JS::GetClass(out)!=&kStyleRuleClass)||
      (conditionOnly&&JS::GetClass(out)!=&kMediaRuleClass&&JS::GetClass(out)!=&kSupportsRuleClass))
    return WebKit::Fail(cx,"CSSRule member called on an incompatible receiver");
  return true;
}
State& ContainerOwnerOf(JS::HandleObject container) {
  return IsRuleObject(container)?RuleOwnerOf(container):SheetOwnerOf(container);
}
std::vector<std::shared_ptr<SheetRule>>& ContainerRows(JS::HandleObject container) {
  return IsRuleObject(container)?RuleDataOf(container).children:SheetDataOf(container).rules;
}
ruyijs::domain::styling::MediaListValue& ContainerMedia(JS::HandleObject container) {
  return IsRuleObject(container)?RuleDataOf(container).media:SheetDataOf(container).media;
}
namespace detail {
JSObject* NewCssRule(State& state,JS::HandleObject parent,size_t index,
                     JS::MutableHandleObject out) {
  const bool nested=IsRuleObject(parent);
  JSContext* cx=state.cx;
  JS::RootedObject wrappers(cx,&(nested?RuleSlotValue(parent,RuleWrappers):JS::GetReservedSlot(parent,SheetWrappers)).toObject());
  JS::RootedValue existing(cx);
  if(!JS_GetElement(cx,wrappers,uint32_t(index),&existing)) return nullptr;
  if(existing.isObject()) { out.set(&existing.toObject());return out; }
  auto holder=std::unique_ptr<std::shared_ptr<SheetRule>>(
      new(std::nothrow) std::shared_ptr<SheetRule>(ContainerRows(parent)[index]));
  if(!holder) { JS_ReportOutOfMemory(cx);return nullptr; }
  const int kind=(*holder)->kind;
  WebFace& face=kind==0?state.nestedDeclarations:kind==4?state.mediaRule:kind==12?state.supportsRule:
    kind==7?state.keyframesRule:kind==8?state.keyframeRule:kind==6?state.pageRule:state.styleRule;
  JS::RootedObject prototype(cx,kind==7?nullptr:face.prototype.get());
  JS::RootedObject object(cx,JS_NewObjectWithGivenProto(cx,kind==7?&kRuleClass:face.clasp,prototype));
  if(!object) return nullptr;
  if(kind==7) {
    JS::RootedValue storage(cx,JS::ObjectValue(*object));js::ProxyOptions options;
    options.setClass(&kKeyframesRuleClass);
    object=js::NewProxyObject(cx,CssListHandler(),storage,face.prototype,options);
    if(!object) return nullptr;
  }
  SetRuleSlotValue(object,RuleData,JS::PrivateValue(holder.release()));
  SetRuleSlotValue(object,RuleOwner,JS::PrivateValue(&state));
  SetRuleSlotValue(object,RuleAncestor,nested?JS::ObjectValue(*parent):JS::NullValue());
  JS::RootedValue sheet(cx,nested?RuleSlotValue(parent,RuleParent):JS::ObjectValue(*parent));
  SetRuleSlotValue(object,RuleParent,sheet);
  JS::RootedObject children(cx,JS::NewArrayObject(cx,RuleDataOf(object).children.size()));
  if(!children||!JS_SetPrototype(cx,children,nullptr)) return nullptr;
  SetRuleSlotValue(object,RuleWrappers,JS::ObjectValue(*children));
  JS::RootedObject list(cx);
  if(!NewCssList(state,object,&list)) return nullptr;
  SetRuleSlotValue(object,RuleList,JS::ObjectValue(*list));
  if(kind==4) {
    if(!NewCssList(state,object,&list,MediaCollection)) return nullptr;
    SetRuleSlotValue(object,RuleMedia,JS::ObjectValue(*list));
  }
  if(!JS_SetElement(cx,wrappers,uint32_t(index),object)) return nullptr;
  out.set(object);return out;
}
}
bool DetachRule(JS::HandleObject rule,bool clearAncestor) {
  JSContext* cx=RuleOwnerOf(rule).cx;
  SetRuleSlotValue(rule,RuleParent,JS::NullValue());
  if(clearAncestor) SetRuleSlotValue(rule,RuleAncestor,JS::NullValue());
  JS::RootedValue value(cx,RuleSlotValue(rule,RuleWrappers));
  if(!value.isObject()) return true;
  JS::RootedObject wrappers(cx,&value.toObject());
  for(uint32_t i=0;i<RuleDataOf(rule).children.size();++i) {
    if(!JS_GetElement(cx,wrappers,i,&value)) return false;
    if(value.isObject()) { JS::RootedObject child(cx,&value.toObject());if(!DetachRule(child,false)) return false; }
  }
  return true;
}
bool ShiftRuleWrappers(JSContext* cx,JS::HandleObject parent,uint32_t index,bool inserting) {
  JS::RootedObject wrappers(cx,&(IsRuleObject(parent)?RuleSlotValue(parent,RuleWrappers):JS::GetReservedSlot(parent,SheetWrappers)).toObject());
  const auto length=uint32_t(ContainerRows(parent).size());
  JS::RootedValue value(cx);
  if(inserting) {
    for(uint32_t i=length;i>index;--i)
      if(!JS_GetElement(cx,wrappers,i-1,&value)||!JS_SetElement(cx,wrappers,i,value)) return false;
    value.setUndefined();
    return JS_SetElement(cx,wrappers,index,value)&&JS::SetArrayLength(cx,wrappers,length+1);
  }
  if(!JS_GetElement(cx,wrappers,index,&value)) return false;
  if(value.isObject()) { JS::RootedObject object(cx,&value.toObject());if(!DetachRule(object)) return false; }
  for(uint32_t i=index;i+1<length;++i)
    if(!JS_GetElement(cx,wrappers,i+1,&value)||!JS_SetElement(cx,wrappers,i,value)) return false;
  return JS::SetArrayLength(cx,wrappers,length-1);
}
}
