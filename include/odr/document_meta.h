#ifndef ODR_DOCUMENT_META_H
#define ODR_DOCUMENT_META_H

#include <odr/document_type.h>
#include <odr/table_dimensions.h>
#include <optional>
#include <string>
#include <vector>

namespace odr {

struct DocumentMeta final {
  DocumentMeta();
  DocumentMeta(DocumentType document_type, std::uint32_t entry_count);

  DocumentType document_type{DocumentType::UNKNOWN};
  std::uint32_t entry_count{0};
};

} // namespace odr

#endif // ODR_DOCUMENT_META_H
