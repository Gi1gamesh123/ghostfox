#include "document_model/model_internal.h"
#include "domain/document_model/meter_values.h"
#include <cmath>

namespace ruyijs::document_model {
Status Model::MeterValue(NodeKey key, DomStringView name, double* out) const noexcept {
  return Protect([&] { *out = domain::meter_value(State().element(key), name); });
}
Status Model::SetMeterValue(NodeKey key, DomStringView name, double value) noexcept {
  DomString text;
  auto status = Protect([&] {
    if (std::isnan(value)) text = u"nan";
    else if (std::isinf(value)) text = value < 0 ? u"-inf" : u"inf";
    else text = domain::from_utf8(std::to_string(value));
  });
  return status.code == ErrorCode::None ? SetAttribute(key, name, text) : status;
}
}
