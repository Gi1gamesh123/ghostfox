// Script policy lives on the original Element, never in a runtime-side index.
#include "document_model/model_internal.h"
namespace ruyijs::document_model {
namespace {
bool ModuleType(DomStringView value) noexcept {
  auto whitespace = [](char16_t c) {
    return c == u'\t' || c == u'\n' || c == u'\f' || c == u'\r' || c == u' ';
  };
  while (!value.empty() && whitespace(value.front())) value.remove_prefix(1);
  while (!value.empty() && whitespace(value.back())) value.remove_suffix(1);
  if (value.size() != 6) return false;
  constexpr char16_t expected[] = u"module";
  for (size_t i = 0; i < 6; ++i) {
    char16_t c = value[i];
    if (c >= u'A' && c <= u'Z') c += u'a' - u'A';
    if (c != expected[i]) return false;
  }
  return true;
}
}
void Model::SetScriptSink(void (*sink)(void*,NodeKey,bool),void* context) noexcept {
  if(impl_){impl_->scriptSink=sink;impl_->scriptSinkContext=context;}
}
Status Model::ScriptAsync(NodeKey key,bool* result) const noexcept {
  return Protect([&]{const auto& e=State().element(key);
    *result=e.script_state().force_async||e.has_attribute_ns16({},u"async");});
}
Status Model::ScriptPosition(NodeKey key,uint32_t* line,uint32_t* column) const noexcept {
  return Protect([&]{const auto& s=State().element(key).script_state();*line=s.line;*column=s.column;});
}
Status Model::SetScriptAsync(NodeKey key,bool value) noexcept {
  auto status=Protect([&]{State().element(key).script_state().force_async=false;});
  if(status.code!=ErrorCode::None)return status;
  return value?SetAttribute(key,u"async",u""):RemoveAttribute(key,u"async");
}
Status Model::PrepareScript(NodeKey key,bool parser,bool* execute,bool* external) noexcept {
  return Protect([&]{
    *execute=false;*external=false;
    auto& owner=State().owner(key);auto* e=owner.get_element(key.node);
    if(!e||!e->tag_name().is_script())return;
    auto& s=e->script_state();
    if(parser)s.parser_inserted=false;
    if(s.started||s.parser_inserted||owner.composed_root_id(key.node)!=owner.id())return;
    const auto src=e->get_attribute_ns16({},u"src");
    const auto type=e->get_attribute_ns16({},u"type").value_or(u"");
    const bool classic=domain::script_classic<char16_t>(type);
    // The first module slice is deliberately limited to parser-inserted,
    // external, non-async scripts. PageLoad remains the sole coordinator for
    // fetching, evaluating, and advancing the document lifecycle.
    const bool module=parser&&ModuleType(type)&&src&&!src->empty()&&
        !e->has_attribute_ns16({},u"async");
    if((!classic&&!module)||(classic&&e->has_attribute_ns16({},u"nomodule")))return;
    if(!src && owner.text_content16(key.node).empty())return;
    s.started=true;*execute=true;*external=bool(src);
  });
}
}
