#include <odr/document_meta.h>

namespace odr {

DocumentMeta::DocumentMeta() = default;

DocumentMeta::DocumentMeta(const DocumentType document_type,
                           const std::uint32_t entry_count)
    : document_type{document_type}, entry_count{entry_count} {}

} // namespace odr
