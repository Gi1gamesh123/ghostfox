#ifndef RUYIJS_MODEL_HEADERS_H
#define RUYIJS_MODEL_HEADERS_H
#include "resource_network/model.h"

namespace ruyijs::resource_network {
struct HeaderArena;
struct HeaderRecord;
struct HeaderDraft;
struct IteratorTicket;
enum class Mutation { Append, Set, Delete };
Status NewArena(HeaderArena**) noexcept;
void Retain(HeaderArena*) noexcept;
void Release(HeaderArena*) noexcept;
Status NewDraft(HeaderDraft**) noexcept;
void DeleteDraft(HeaderDraft*) noexcept;
Status DraftTuple(HeaderDraft*) noexcept;
Status DraftValue(HeaderDraft*, std::string_view) noexcept;
Status CreateHeaders(HeaderArena*, HeaderDraft*, HeaderRecord**) noexcept;
void DeleteHeaders(HeaderRecord*) noexcept;
void MakeHeadersImmutable(HeaderRecord*) noexcept;
bool HeadersImmutable(const HeaderRecord*) noexcept;
Status ByteString(const char16_t*, size_t, std::string*) noexcept;
Status Mutate(HeaderRecord*, Mutation, std::string_view, std::string_view = {}) noexcept;
Status SnapshotHeaders(HeaderRecord*,HeaderPairs*) noexcept;
Status FilterResponseHeaders(const HeaderPairs&,bool crossOrigin,std::string_view mode,
                            bool credentialsInclude,HeaderPairs*) noexcept;
Status Get(HeaderRecord*, std::string_view, std::string*, bool*) noexcept;
Status Has(HeaderRecord*, std::string_view, bool*) noexcept;
Status Cookies(HeaderRecord*, std::vector<std::string>*) noexcept;
Status Entry(HeaderRecord*, size_t, std::string*, std::string*, bool*) noexcept;
Status NewTicket(HeaderRecord*, IteratorTicket**) noexcept;
void DeleteTicket(IteratorTicket*) noexcept;
}
#endif
