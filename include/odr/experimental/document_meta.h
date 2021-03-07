#ifndef ODR_EXPERIMENTAL_DOCUMENT_META_H
#define ODR_EXPERIMENTAL_DOCUMENT_META_H

#include <odr/document_type.h>
#include <odr/table_dimensions.h>
#include <optional>
#include <string>
#include <vector>

namespace odr::experimental {

struct DocumentMeta final {
  struct Entry {
    std::optional<std::string> name;
    std::optional<TableDimensions> table_dimensions;
    std::optional<std::string> notes;

    Entry();
  };

  DocumentMeta();
  DocumentMeta(DocumentType document_type, std::uint32_t entry_count,
               std::vector<Entry> entries);

  DocumentType document_type{DocumentType::UNKNOWN};
  std::uint32_t entry_count{0};
  std::vector<Entry> entries;
};

} // namespace odr::experimental

#endif // ODR_EXPERIMENTAL_DOCUMENT_META_H
