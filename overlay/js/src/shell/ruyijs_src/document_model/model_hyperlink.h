#ifndef RUYIJS_DOCUMENT_HYPERLINK_H
#define RUYIJS_DOCUMENT_HYPERLINK_H
#include "document_model/model.h"
#include <string>

namespace ruyijs::document_model {
enum class HyperlinkField { Href, Origin, Protocol, Host, Hostname, Port, Pathname, Search, Hash, Username, Password };
// Pure adapters over the existing URL value object; no URL state is retained.
Status ReadHyperlink(const std::string& href, const std::string& base,
                     HyperlinkField field, std::string* result) noexcept;
Status WriteHyperlink(const std::string& href, const std::string& base,
                      HyperlinkField field, const std::string& value,
                      std::string* result, bool* changed) noexcept;
}
#endif
