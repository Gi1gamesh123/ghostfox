#ifndef RUYIJS_DOCUMENT_MODEL_TRANSFER_VALUES_H
#define RUYIJS_DOCUMENT_MODEL_TRANSFER_VALUES_H
#include "document_model/model_internal.h"

namespace ruyijs::document_model {
inline void PreserveElementState(const domain::DocumentAggregate& from, domain::DocumentAggregate& to,
                       const std::vector<domain::NodeId>& written, const std::vector<domain::NodeId>& made) {
  for (size_t i = 0; i < written.size(); ++i) {
    const auto* source = from.get_element(written[i]);
    if (!source) continue;
    auto* target = to.get_element(made[i]);
    if (source->has_svg_view_box()) target->preserve_svg_view_box(source->svg_view_box());
    // Adoption keeps the original point records (and their JS identities) and typed lengths.
    if (source->has_svg_points()) target->preserve_svg_points(source->svg_points());
    target->preserve_svg_lengths(*source);
    if (!source->tag_name().html()) continue;
    if(source->tag_name().is_script())target->script_state()=source->script_state();
    if (source->tag_name().local16() == u"input") {
      target->input_current_value() = source->input_current_value();
      target->input_check_state() = source->input_check_state();
      target->set_custom_validity(source->custom_validity());
    } else if (source->tag_name().local16() == u"textarea") {
      target->textarea_state() = source->textarea_state();
      target->set_custom_validity(source->custom_validity());
    } else if (source->tag_name().local16() == u"dialog")
      target->set_dialog_return_value(source->dialog_return_value());
    else if (source->tag_name().local16() == u"template")
      target->set_template_content(source->template_content());
  }
}
// A cross-document copy is shape-identical to its source, so the pre-order
// traversals of the source and of the copy line up one to one (index i of one
// is the clone of index i of the other). This is the only place that relies on
// it; the realm repoints its wrappers, facades, frames and collections with the
// pairs so the caller keeps the JS object identity it already holds.
inline void PairIdentities(const std::vector<domain::NodeId>& written,
                    const std::vector<domain::NodeId>& made, DocumentId from, DocumentId to,
                    std::vector<std::pair<NodeKey, NodeKey>>* rekey) {
  if (!rekey) return;
  rekey->reserve(rekey->size() + written.size());
  const size_t count = written.size() < made.size() ? written.size() : made.size();
  for (size_t index = 0; index < count; ++index)
    rekey->push_back({Key(from, written[index]), Key(to, made[index])});
}
}
#endif
