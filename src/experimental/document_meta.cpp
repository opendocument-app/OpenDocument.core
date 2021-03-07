#include <odr/experimental/document_meta.h>

namespace odr::experimental {

DocumentMeta::Entry::Entry() = default;

DocumentMeta::DocumentMeta() = default;

DocumentMeta::DocumentMeta(const DocumentType document_type,
                           const std::uint32_t entry_count,
                           std::vector<Entry> entries)
    : document_type{document_type}, entry_count{entry_count}, entries{std::move(
                                                                  entries)} {}

} // namespace odr::experimental
