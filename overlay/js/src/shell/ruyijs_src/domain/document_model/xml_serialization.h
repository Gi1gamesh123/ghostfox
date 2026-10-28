#pragma once
#include "domain/document_model/document.h"
#include "domain/document_model/xml_namespaces.h"

namespace ruyijs::domain::document_model {
inline DomString escape_xml(DomStringView value, bool attribute) {
  DomString out;
  for (auto c : value) {
    if (c == u'&') out += u"&amp;";
    else if (c == u'<') out += u"&lt;";
    else if (c == u'>') out += u"&gt;";
    else if (attribute && c == u'\"') out += u"&quot;";
    else if (attribute && c == u'\n') out += u"&#xA;";
    else if (attribute && c == u'\r') out += u"&#xD;";
    else if (attribute && c == u'\t') out += u"&#9;";
    else out += c;
  }
  return out;
}
inline void xml_attribute(DomString& out, DomStringView name, DomStringView value) {
  out += u" " + DomString(name) + u"=\"" + escape_xml(value, true) + u"\"";
}
inline void xml_namespace(DomString& out, XmlNamespaces& namespaces,
                          DomStringView prefix, DomStringView uri) {
  xml_attribute(out, prefix.empty() ? u"xmlns" : xml_qualified(u"xmlns", prefix), uri);
  namespaces.push(prefix, uri);
}
inline DomString xml_start_tag(const Element& element, XmlNamespaces& namespaces, DomString& name) {
  const auto& tag = element.tag_name();
  const auto scope = namespaces.size();
  const Attribute* skipped = nullptr;
  for (const auto& attr : element.attributes16()) {
    if (attr.namespace_uri == kXmlnsNamespace || (attr.namespace_uri.empty() && attr.name == u"xmlns")) {
      if (attr.prefix.empty() && tag.namespace16().empty() && !attr.value.empty()) skipped = &attr;
      else namespaces.push(attr.prefix.empty() ? DomStringView() : DomStringView(attr.local_name), attr.value);
    }
  }
  DomString prefix(tag.prefix16());
  const bool declare = namespaces.confirm(prefix, tag.namespace16(), scope, false);
  name = xml_qualified(prefix, tag.local16());
  DomString out = u"<" + name;
  if (element.is_value() && !element.has_attribute_ns16({}, u"is"))
    xml_attribute(out, u"is", *element.is_value());
  if (declare) xml_namespace(out, namespaces, prefix, tag.namespace16());
  for (const auto& attr : element.attributes16()) {
    if (&attr == skipped) continue;
    DomString attribute_prefix = attr.prefix;
    const bool add = attr.namespace_uri != kXmlnsNamespace &&
        namespaces.confirm(attribute_prefix, attr.namespace_uri, scope, true);
    xml_attribute(out, xml_qualified(attribute_prefix, attr.local_name), attr.value);
    if (add) xml_namespace(out, namespaces, attribute_prefix, attr.namespace_uri);
  }
  return out;
}
inline DomString serialize_xml(const DocumentAggregate& doc, NodeId root) {
  struct Entry { NodeId key; bool close = false; size_t restore = 0; DomString name; };
  XmlNamespaces namespaces;
  std::vector<Entry> stack{{root}};
  DomString output;
  while (!stack.empty()) {
    auto entry = std::move(stack.back()); stack.pop_back();
    if (entry.close) {
      output += u"</" + entry.name + u">";
      namespaces.restore(entry.restore); continue;
    }
    const auto& node = doc.require(entry.key);
    const auto* element = doc.get_element(entry.key);
    if (node.node_type() == NodeType::Text) { output += escape_xml(node.text_content16(), false); continue; }
    if (node.node_type() == NodeType::Comment) { output += u"<!--" + node.text_content16() + u"-->"; continue; }
    if (node.node_type() == NodeType::ProcessingInstruction) {
      const auto& pi = static_cast<const ProcessingInstructionNode&>(node);
      const DomString data = pi.text_content16();
      output += u"<?" + pi.target() + (data.empty() ? u"" : u" " + data) + u"?>"; continue;
    }
    if (node.node_type() == NodeType::DocumentType) { output += u"<!DOCTYPE " + node.node_name16() + u">"; continue; }
    if (element) {
      const size_t restore = namespaces.size();
      DomString name;
      output += xml_start_tag(*element, namespaces, name);
      if (!node.child_count() && (!element->tag_name().html() || element->tag_name().is_void())) {
        output += element->tag_name().html() ? u" />" : u"/>";
        namespaces.restore(restore);
        continue;
      }
      output += u">";
      stack.push_back({entry.key, true, restore, std::move(name)});
    }
    for (auto it = node.child_ids().rbegin(); it != node.child_ids().rend(); ++it)
      stack.push_back({*it});
  }
  return output;
}
}
