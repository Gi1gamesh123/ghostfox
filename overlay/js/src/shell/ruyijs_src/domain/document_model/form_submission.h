#pragma once
#include "domain/document_model/form_tree.h"
#include "domain/document_model/textarea_values.h"

namespace ruyijs::domain::document_model {
struct FormEntry { DomString name, value; bool empty_file = false; };
inline bool form_disabled(const DocumentAggregate& doc, NodeId id) {
  const auto* e = doc.get_element(id);
  if (!e) return false;
  const auto& tag = e->tag_name().local16();
  if (tag != u"input" && tag != u"button" && tag != u"select" && tag != u"textarea" &&
      tag != u"fieldset" && tag != u"option" && tag != u"optgroup") return false;
  if (e->get_attribute16(u"disabled")) return true;
  for (auto parent = e->parent_id(); parent; parent = doc.require(parent).parent_id()) {
    const auto* fieldset = doc.get_element(parent);
    if (!form_tag(doc, parent, u"fieldset") || !fieldset->get_attribute16(u"disabled")) continue;
    NodeId legend = 0;
    for (auto child : fieldset->child_ids()) if (form_tag(doc, child, u"legend")) { legend = child; break; }
    bool exempt = false;
    for (auto p = id; p; p = doc.require(p).parent_id()) if (p == legend) exempt = true;
    if (!exempt) return true;
  }
  return false;
}
inline bool form_submit_button(const DocumentAggregate& doc, NodeId id) {
  const auto* e = doc.get_element(id);
  if (!e) return false;
  if (form_tag(doc, id, u"input")) return input_type(*e) == u"submit" || input_type(*e) == u"image";
  if (!form_tag(doc, id, u"button")) return false;
  const auto type = ascii_lower(e->get_attribute16(u"type").value_or(u""));
  return type != u"button" && type != u"reset";
}
inline std::vector<FormEntry> form_entries(const DocumentAggregate& doc, NodeId form, NodeId submitter) {
  std::vector<FormEntry> out;
  for (auto id : form_nodes(doc, form)) {
    const auto& e = *doc.get_element(id);
    const auto tag = e.tag_name().local16();
    if (form_disabled(doc, id)) continue;
    bool datalist = false;
    for (auto p = e.parent_id(); p; p = doc.require(p).parent_id()) if (form_tag(doc, p, u"datalist")) datalist = true;
    if (datalist) continue;
    const auto name = e.get_attribute16(u"name").value_or(u"");
    if (tag == u"input" && input_type(e) == u"image") {
      if (id == submitter) {
        out.push_back({name.empty() ? u"x" : name + u".x", u"0"});
        out.push_back({name.empty() ? u"y" : name + u".y", u"0"});
      }
      continue;
    }
    if (name.empty()) continue;
    if (tag == u"button") {
      if (id == submitter) out.push_back({name, e.get_attribute16(u"value").value_or(u"")});
    } else if (tag == u"input") {
      const auto type = input_type(e);
      if ((type == u"checkbox" || type == u"radio") && !e.input_checked()) continue;
      if (type == u"reset" || type == u"button" || (type == u"submit" && id != submitter)) continue;
      auto value = type == u"hidden" && name == u"_charset_" ? u"UTF-8" : input_value(e);
      out.push_back({name, value, type == u"file"});
    } else if (tag == u"textarea") out.push_back({name, textarea_value(doc, e)});
    else if (tag == u"select") {
      for (auto option : doc.descendants(id)) {
        if (!form_tag(doc, option, u"option")) continue;
        const auto& o = *doc.get_element(option);
        if (!o.option_selected() || o.get_attribute16(u"disabled")) continue;
        auto* group = doc.get_element(o.parent_id());
        if (group && form_tag(doc, group->id(), u"optgroup") && group->get_attribute16(u"disabled")) continue;
        out.push_back({name, o.get_attribute16(u"value").value_or(doc.text_content16(option))});
      }
    }
    if (tag == u"textarea" || (tag == u"input" && (input_type(e) == u"text" || input_type(e) == u"search"))) {
      const auto dirname = e.get_attribute16(u"dirname").value_or(u"");
      if (!dirname.empty()) {
        DomString direction = u"ltr";
        for (auto p = id; p; p = doc.require(p).parent_id()) {
          auto* ancestor = doc.get_element(p);
          auto dir = ancestor ? ascii_lower(ancestor->get_attribute16(u"dir").value_or(u"")) : u"";
          if (dir == u"rtl" || dir == u"ltr") { direction = dir; break; }
        }
        out.push_back({dirname, direction});
      }
    }
  }
  return out;
}
}
